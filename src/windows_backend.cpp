#include "backend.h"
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <ntddvdeo.h>
#include <nvapi.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>

namespace {
using kvm::Code;
using kvm::Error;
void nvcheck(NvAPI_Status status, const char* operation, Code code=Code::backend) {
    if(status!=NVAPI_OK) throw Error(code,std::string(operation)+" failed (NVAPI status "+std::to_string(status)+").");
}
std::string utf8(const wchar_t* text) {
    const int required=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr);
    if(required<=0) throw Error(Code::discovery,"Cannot encode display name.");
    // Include space for the terminating NUL passed to WideCharToMultiByte.
    std::string result(static_cast<std::size_t>(required),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,result.data(),required,nullptr,nullptr)!=required)
        throw Error(Code::discovery,"Cannot encode display name.");
    result.pop_back(); return result;
}
struct DeviceSet {
    HDEVINFO handle=SetupDiGetClassDevsW(&GUID_DEVINTERFACE_MONITOR,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    ~DeviceSet() { if(handle!=INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(handle); }
};
struct RegistryKey {
    HKEY handle=nullptr;
    ~RegistryKey() { if(handle && handle!=reinterpret_cast<HKEY>(INVALID_HANDLE_VALUE)) RegCloseKey(handle); }
};
std::array<std::uint8_t,128> read_edid(const std::wstring& path) {
    DeviceSet devices;
    if(devices.handle==INVALID_HANDLE_VALUE) throw Error(Code::discovery,"Monitor device enumeration failed.");
    for(DWORD index=0;;++index) {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize=sizeof(iface);
        if(!SetupDiEnumDeviceInterfaces(devices.handle,nullptr,&GUID_DEVINTERFACE_MONITOR,index,&iface)) {
            if(GetLastError()==ERROR_NO_MORE_ITEMS) break;
            throw Error(Code::discovery,"Monitor interface enumeration failed.");
        }
        DWORD required=0;
        SetupDiGetDeviceInterfaceDetailW(devices.handle,&iface,nullptr,0,&required,nullptr);
        if(required<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        // Allocate naturally aligned storage for the variable-length structure.
        std::vector<std::uint64_t> storage((required+7)/8);
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA info{}; info.cbSize=sizeof(info);
        if(!SetupDiGetDeviceInterfaceDetailW(devices.handle,&iface,detail,required,nullptr,&info)) continue;
        if(_wcsicmp(path.c_str(),detail->DevicePath)!=0) continue;
        RegistryKey key;
        key.handle=SetupDiOpenDevRegKey(devices.handle,&info,DICS_FLAG_GLOBAL,0,DIREG_DEV,KEY_READ);
        if(key.handle==reinterpret_cast<HKEY>(INVALID_HANDLE_VALUE)) break;
        DWORD type=0, bytes=0;
        if(RegQueryValueExW(key.handle,L"EDID",nullptr,&type,nullptr,&bytes)!=ERROR_SUCCESS || type!=REG_BINARY || bytes<128) break;
        std::vector<BYTE> edid(bytes);
        if(RegQueryValueExW(key.handle,L"EDID",nullptr,&type,edid.data(),&bytes)!=ERROR_SUCCESS || bytes<128) break;
        std::array<std::uint8_t,128> base{};
        std::copy_n(edid.begin(),128,base.begin()); return base;
    }
    return {}; // Visible in list, but cannot match the allowlist.
}
class WindowsBackend final : public kvm::Backend {
    bool initialized_=false;
    struct Route { NvPhysicalGpuHandle gpu; NvU32 output; };
    Route route(const kvm::Monitor& monitor) {
        if(!initialized_) { nvcheck(NvAPI_Initialize(),"NvAPI_Initialize"); initialized_=true; }
        NvDisplayHandle display=nullptr;
        nvcheck(NvAPI_GetAssociatedNvidiaDisplayHandle(monitor.source.c_str(),&display),"NVIDIA display mapping");
        NvPhysicalGpuHandle gpus[NVAPI_MAX_PHYSICAL_GPUS]{};
        NvU32 count=0;
        nvcheck(NvAPI_GetPhysicalGPUsFromDisplay(display,gpus,&count),"NVIDIA GPU mapping");
        if(count!=1) throw Error(Code::identity,"Display must map to exactly one NVIDIA physical GPU.");
        NvU32 output=0;
        nvcheck(NvAPI_GetAssociatedDisplayOutputId(display,&output),"NVIDIA output mapping");
        if(output==0 || (output&(output-1))!=0) throw Error(Code::identity,"NVIDIA output mask is absent or ambiguous.");
        NV_EDID edid{}; edid.version=NV_EDID_VER;
        nvcheck(NvAPI_GPU_GetEDID(gpus[0],output,&edid),"NVIDIA EDID read");
        if(!std::equal(monitor.edid.begin(),monitor.edid.end(),edid.EDID_Data))
            throw Error(Code::identity,"NVIDIA output EDID differs from Windows monitor EDID.");
        return {gpus[0],output};
    }
public:
    ~WindowsBackend() override { if(initialized_) NvAPI_Unload(); }
    std::vector<kvm::Monitor> discover() override {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths;
        std::vector<DISPLAYCONFIG_MODE_INFO> modes;
        LONG status=ERROR_INSUFFICIENT_BUFFER;
        for(int attempt=0; attempt<3 && status==ERROR_INSUFFICIENT_BUFFER; ++attempt) {
            UINT32 path_count=0,mode_count=0;
            status=GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&path_count,&mode_count);
            if(status!=ERROR_SUCCESS) break;
            paths.resize(path_count); modes.resize(mode_count);
            status=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&path_count,paths.data(),&mode_count,modes.data(),nullptr);
            if(status==ERROR_SUCCESS) paths.resize(path_count);
        }
        if(status!=ERROR_SUCCESS) throw Error(Code::discovery,"Active display configuration unavailable (Windows error "+std::to_string(status)+").");
        std::vector<kvm::Monitor> result;
        std::map<std::string,unsigned> source_counts;
        for(const auto& path:paths) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size=sizeof(source);
            source.header.adapterId=path.sourceInfo.adapterId;
            source.header.id=path.sourceInfo.id;
            DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
            target.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            target.header.size=sizeof(target);
            target.header.adapterId=path.targetInfo.adapterId;
            target.header.id=path.targetInfo.id;
            if(DisplayConfigGetDeviceInfo(&source.header)!=ERROR_SUCCESS ||
               DisplayConfigGetDeviceInfo(&target.header)!=ERROR_SUCCESS)
                throw Error(Code::discovery,"Cannot identify an active display path.");
            kvm::Monitor monitor;
            monitor.source=utf8(source.viewGdiDeviceName);
            monitor.device_path=target.monitorDevicePath;
            monitor.display_port=path.targetInfo.outputTechnology==DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EXTERNAL;
            monitor.edid=read_edid(monitor.device_path);
            ++source_counts[monitor.source];
            result.push_back(std::move(monitor));
        }
        for(auto& monitor:result) monitor.unique_route=source_counts.at(monitor.source)==1;
        return result;
    }
    std::string probe(const kvm::Monitor& monitor) override {
        const auto r=route(monitor);
        std::ostringstream out;
        out<<"backend=NVIDIA NVAPI output-mask=0x"<<std::hex<<r.output
           <<" EDID-route-match=yes; I2C write capability remains untested";
        return out.str();
    }
    void send(const kvm::Monitor& monitor,const kvm::Packet& packet) override {
        // Recheck GPU routing and its read-only EDID immediately before one write.
        const auto r=route(monitor);
        auto source=packet.source;
        auto payload=packet.payload;
        NV_I2C_INFO info{};
        info.version=NV_I2C_INFO_VER;
        info.displayMask=r.output;
        info.bIsDDCPort=1;
        info.i2cDevAddress=packet.device;
        info.pbI2cRegAddress=&source;
        info.regAddrSize=1;
        info.pbData=payload.data();
        info.cbSize=static_cast<NvU32>(payload.size());
        info.i2cSpeed=NVAPI_I2C_SPEED_DEPRECATED;
        info.i2cSpeedKhz=NVAPI_I2C_SPEED_33KHZ;
        nvcheck(NvAPI_I2CWrite(r.gpu,&info),"NvAPI_I2CWrite (no retries; inspect the monitor before retrying)",Code::write);
    }
};
}
std::unique_ptr<kvm::Backend> make_backend() { return std::make_unique<WindowsBackend>(); }
