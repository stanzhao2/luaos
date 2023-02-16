

#ifndef __SMBIOS_H
#define __SMBIOS_H

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

////////////////////////////////////////////////////////////////////////////////

struct SMB_HDR
{
    BYTE	bySection;
    BYTE	byLength;	
    WORD	wHandle;
};

struct SMB_Base
{
    SMB_HDR	stHeader;
};

struct SMB_TYPE0 : public SMB_Base
{
    BYTE	byVendor;
    BYTE	byBiosVersion;
    WORD	wBIOSStartingSegment;
    BYTE	byBIOSReleaseDate;
    BYTE	byBIOSROMSize;
    BYTE	qwBIOSCharacteristics[8];
    BYTE	byExtensionByte1;
    BYTE	byExtensionByte2;
    BYTE	bySystemBIOSMajorRelease;
    BYTE	bySystemBIOSMinorRelease;
    BYTE	byEmbeddedFirmwareMajorRelease;
    BYTE	byEmbeddedFirmwareMinorRelease;
    char*	szVendor;
    char*	szBIOSVersion;
    char*	szBIOSReleaseDate;
};

struct SMB_TYPE1 : public SMB_Base
{
    BYTE	byManufacturer;
    BYTE	byProductName;
    BYTE	byVersion;
    BYTE	bySerialNumber;
    BYTE	byUUID[16];
    BYTE	byWakeupType;
    BYTE	bySKUNumber;
    BYTE	byFamily;
    char*	szManufacturer;
    char*	szProductName;
    char*	szVersion;
    char*	szSerialNumber;
    char*	szSKUNumber;
    char*	szFamily;
};

struct SMB_TYPE2 : public SMB_Base
{
    BYTE	byManufacturer;
    BYTE	byProductName;
    BYTE	byVersion;
    BYTE	bySerialNumber;
    BYTE	byAssetTag;
    BYTE	byFeatureFlags;
    BYTE	byLocationInChassis;
    WORD	wChassisHandle;
    BYTE	byBoardType;
    BYTE	byNoOfContainedObjectHandles;
    WORD*	ContainedObjectHandles[255];
    char*	szManufacturer;
    char*	szProductName;
    char*	szVersion;
    char*	szSerialNumber;
    char*	szAssetTag;
    char*	szLocationInChassis;
};

struct SMB_TYPE3 : public SMB_Base
{
    BYTE	byManufacturer;
    BYTE	byType;
    BYTE	byVersion;
    BYTE	bySerialNumber;
    BYTE	byAssetTag;
    BYTE	byBootupState;
    BYTE	byPowerSupplyState;
    BYTE	byThermalState;
    BYTE	bySecurityStatus;
    DWORD	dwOEMdefined;
    BYTE	byHeight;
    BYTE	byNumberOfPowerCords;
    BYTE	byContainedElementCount;
    BYTE	byContainedElementRecordLength;
    char*	szManufacturer;
    char*	szVersion;
    char*	szSerialNumber;
    char*	szAssetTag;
};

struct SMB_TYPE4 : public SMB_Base
{
    BYTE	bySocketDesignation;
    BYTE	byProcessorType;
    BYTE	byProcessorFamily;
    BYTE	byProcessorManufacturer;
    BYTE	qwProcessorID[8];
    BYTE	byProcessorVersion;
    BYTE	byVoltage;
    WORD	wExternalClock;
    WORD	wMaxSpeed;
    WORD	wCurrentSpeed;
    BYTE	byStatus;
    BYTE	byProcessorUpgrade;
    WORD	wL1CacheHandle;
    WORD	wL2CacheHandle;
    WORD	wL3CacheHandle;
    BYTE	bySerialNumber;
    BYTE	byAssetTagNumber;
    BYTE	byPartNumber;
    char*	szSocketDesignation;
    char*	szProcessorManufacturer;
    char*	szProcessorVersion;
    char*	szSerialNumber;
    char*	szAssetTagNumber;
    char*	szPartNumber;
};

////////////////////////////////////////////////////////////////////////////////

class SMBIOS
{
public:
    SMBIOS();
    virtual ~SMBIOS();
    BOOL  Init();
    BOOL  GetData(SMB_TYPE0& out);	//BIOS Information.
    BOOL  GetData(SMB_TYPE1& out);	//System Information.
    BOOL  GetData(SMB_TYPE2& out);	//Base Board Information.
    BOOL  GetData(SMB_TYPE3& out);	//System Enclosure or Chassis Type.
    BOOL  GetData(SMB_TYPE4& out);	//Processor Information.

private:
    LPSTR GetString(const BYTE* pbData, BYTE byIndex);
    BYTE* FetchNext(BYTE* const pbData, BOOL bIgnoreTableType, BYTE byTableType,DWORD& dwTotalTableSize);

private:
    BYTE  m_byMajorVersion;
    BYTE  m_byMinorVersion;
    BYTE* m_pbBIOSData;
    DWORD m_dwLen;
};

////////////////////////////////////////////////////////////////////////////////

#include <string>

FORCEINLINE BOOL GetMachineInfo(char *out, size_t size)
{
    SMBIOS bios;
    if (!bios.Init())
        return FALSE;

    SMB_TYPE0 d0;
    SMB_TYPE1 d1;
    SMB_TYPE2 d2;
    SMB_TYPE3 d3;
    SMB_TYPE4 d4;

    bios.GetData(d0);
    bios.GetData(d1);
    bios.GetData(d2);
    bios.GetData(d3);
    bios.GetData(d4);

    std::string data;
    char buffer[1024];

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %s\r\n"
        , "-Product Name"
        , d1.szProductName ? d1.szProductName : "None"
        );
    data += buffer;

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %s\r\n"
        , "-Product Serial Number"
        , d1.szSerialNumber ? d1.szSerialNumber : "None"
        );
    data += buffer;

    DWORD  v1 = *(DWORD*)d1.byUUID;
    USHORT v2 = *(USHORT*)(d1.byUUID + sizeof(DWORD));
    USHORT v3 = *(USHORT*)(d1.byUUID + sizeof(DWORD) + sizeof(USHORT));

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\r\n"
        , "-Product UUID"
        , v1, v2, v3
        , d1.byUUID[8]
    , d1.byUUID[9]
    , d1.byUUID[10]
    , d1.byUUID[11]
    , d1.byUUID[12]
    , d1.byUUID[13]
    , d1.byUUID[14]
    , d1.byUUID[15]
    );
    data += buffer;

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %s\r\n"
        , "-BIOS Vendor"
        , d0.szVendor ? d0.szVendor : "None"
        );
    data += buffer;

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %s\r\n"
        , "-BIOS Version"
        , d0.szBIOSVersion ? d0.szBIOSVersion : "None"
        );
    data += buffer;

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\r\n"
        , "-BIOS Characteristics"
        , d0.qwBIOSCharacteristics[0]
    , d0.qwBIOSCharacteristics[1]
    , d0.qwBIOSCharacteristics[2]
    , d0.qwBIOSCharacteristics[3]
    , d0.qwBIOSCharacteristics[4]
    , d0.qwBIOSCharacteristics[5]
    , d0.qwBIOSCharacteristics[6]
    , d0.qwBIOSCharacteristics[7]
    );
    data += buffer;

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %s\r\n"
        , "-Baseboard Name"
        , d2.szProductName ? d2.szProductName : "None"
        );
    data += buffer;

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %s\r\n"
        , "-Baseboard Serial Number"
        , d2.szSerialNumber ? d2.szSerialNumber : "None"
        );
    data += buffer;

    sprintf_s(buffer, sizeof(buffer)
        , "%-30s: %s\r\n"
        , "-System Chassis Serial Number"
        , d3.szSerialNumber ? d3.szSerialNumber : "None"
        );
    data += buffer;

    DWORD id[2];
    memcpy(id, d4.qwProcessorID, sizeof(id));

    sprintf_s(buffer, sizeof(buffer), "%-30s: %08X-%08X", "-CPU Serial Number", id[0], id[1]);
    data += buffer;

    if (data.size() + 1 > size)
        return FALSE;

    memcpy(out, data.c_str(), data.size() + 1);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

#endif //__SMBIOS_H
