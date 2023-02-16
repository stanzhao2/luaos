

#include "bios.h"
#include <comdef.h>
#include <wbemidl.h>

////////////////////////////////////////////////////////////////////////////////

SMBIOS::SMBIOS()
{
    m_byMajorVersion = 0;
    m_byMinorVersion = 0;
    m_pbBIOSData = NULL;
    m_dwLen = 0;
}

////////////////////////////////////////////////////////////////////////////////

SMBIOS::~SMBIOS()
{
    if(m_pbBIOSData)
    {
        delete[] m_pbBIOSData;
    }
}

////////////////////////////////////////////////////////////////////////////////

BOOL SMBIOS::Init()
{
    BOOL bRet = FALSE;
    HRESULT hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
    if (FAILED(hres))
    {
        return FALSE;	// Program has failed.
    }

    IWbemLocator *pLoc = 0;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres))
    {
        CoUninitialize();
        return FALSE;	// Program has failed.
    }

    IWbemServices *pSvc = 0;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc); 
    if (FAILED(hres))
    {
        pLoc->Release();     
        CoUninitialize();
        return FALSE;	// Program has failed.
    }

    hres = CoSetProxyBlanket(
        pSvc,							// the proxy to set
        RPC_C_AUTHN_WINNT,				// authentication service
        RPC_C_AUTHZ_NONE,				// authorization service
        NULL,							// Server principal name
        RPC_C_AUTHN_LEVEL_CALL,			// authentication level
        RPC_C_IMP_LEVEL_IMPERSONATE,	// impersonation level
        NULL,							// client identity 
        EOAC_NONE						// proxy capabilities     
        );

    if (FAILED(hres))
    {
        pSvc->Release();
        pLoc->Release();     
        CoUninitialize();
        return FALSE;	// Program has failed.
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->CreateInstanceEnum((BSTR)L"MSSMBios_RawSMBiosTables", 0, NULL, &pEnumerator);
    if (FAILED(hres))
    {
        pSvc->Release();
        pLoc->Release();     
        CoUninitialize();
        return FALSE;	// Program has failed.
    }
    else
    { 
        do
        {
            ULONG dwCount = NULL;
            IWbemClassObject* pInstance = NULL;
            hres = pEnumerator->Next(WBEM_INFINITE, 1, &pInstance, &dwCount);

            if(SUCCEEDED(hres))
            {
                VARIANT varBIOSData;
                VariantInit(&varBIOSData);
                CIMTYPE type;

                hres = pInstance->Get(bstr_t("SmbiosMajorVersion"),0,&varBIOSData,&type,NULL);
                if (!FAILED(hres))
                {
                    m_byMajorVersion = (BYTE)varBIOSData.iVal;
                    VariantInit(&varBIOSData);
                    hres = pInstance->Get(bstr_t("SmbiosMinorVersion"),0,&varBIOSData,&type,NULL);
                    
                    if (!FAILED(hres))
                    {
                        m_byMinorVersion = (BYTE)varBIOSData.iVal;
                        VariantInit(&varBIOSData);
                        hres = pInstance->Get(bstr_t("SMBiosData"),0,&varBIOSData,&type,NULL);
                        
                        if (SUCCEEDED(hres))
                        {
                            if ((VT_UI1 | VT_ARRAY) == varBIOSData.vt)
                            {
                                SAFEARRAY *parray = NULL;
                                parray = V_ARRAY(&varBIOSData);
                                BYTE* pbData = (BYTE*)parray->pvData;

                                m_dwLen = parray->rgsabound[0].cElements;
                                m_pbBIOSData = new BYTE[m_dwLen];
                                memcpy(m_pbBIOSData,pbData,m_dwLen);

                                bRet = TRUE;
                            }
                        }
                    }
                    VariantClear(&varBIOSData);
                }

                VariantClear(&varBIOSData);
                break;
            }
        }
        while(hres == WBEM_S_NO_ERROR);
    }
    return bRet;
}

////////////////////////////////////////////////////////////////////////////////

BYTE* SMBIOS::FetchNext(BYTE* const pbData, BOOL bIgnoreTableType, BYTE byTableType,DWORD& dwTotalTableSize)
{
    BYTE	byCurrentSection = 0;
    BYTE	byLengthOfFormattedSection = 0;
    BYTE*	pbNextTable = NULL;
    BYTE*	pbCurrentTable = NULL;

    pbCurrentTable = (pbData) ? pbData : m_pbBIOSData;

    int j = (int)(pbCurrentTable - m_pbBIOSData);
    for (; j < (int)m_dwLen;)
    {
        byCurrentSection = pbCurrentTable[0];

        if ((FALSE == bIgnoreTableType && byCurrentSection == byTableType && pbCurrentTable != pbData) ||
            (TRUE == bIgnoreTableType && pbCurrentTable != pbData))
        {
            byLengthOfFormattedSection = pbCurrentTable[1];
            BYTE* pUnformattedSection = pbCurrentTable + byLengthOfFormattedSection;

            //now look for 2 zeroes.
            for (int m = 0 ; ; m++)
            {
                if (pUnformattedSection[m] == 0 && pUnformattedSection[m+1] == 0)
                {
                    dwTotalTableSize = byLengthOfFormattedSection + m + 1;
                    break;
                }
            }
            pbNextTable = pbCurrentTable;
            break;
        }

        byLengthOfFormattedSection = pbCurrentTable[1];
        BYTE* pUnformattedSection = pbCurrentTable + byLengthOfFormattedSection;

        //now look for 2 zeroes.
        for (int m = 0 ; ; m++)
        {
            if (pUnformattedSection[m] == 0 && pUnformattedSection[m+1] == 0)
            {
                j = j + byLengthOfFormattedSection + m + 2;
                dwTotalTableSize = byLengthOfFormattedSection + m + 1;
                pbCurrentTable = pbCurrentTable + byLengthOfFormattedSection + m + 2;
                break;
            }
        }

    }
    return pbNextTable;
}

////////////////////////////////////////////////////////////////////////////////

LPSTR SMBIOS::GetString(const BYTE* pbData, BYTE byIndex)
{
    BYTE j = 0;
    char* pStr = NULL;

    do
    {
        pStr = (char*)pbData;
        pbData += (strlen(pStr) + 1);
        j++;
    }
    while(j < byIndex);

    return pStr;
}

////////////////////////////////////////////////////////////////////////////////

//#### BIOS Information ####
BOOL SMBIOS::GetData(SMB_TYPE0& out)
{
    DWORD dwTableSize;
    BYTE* pbData = FetchNext(NULL, FALSE, 0, dwTableSize);
    BYTE* pbSectionStart = pbData;

    memset(&out, 0, sizeof(out));

    if(pbData)
    {
        //mandatory header
        out.stHeader.bySection		= *pbData++;
        out.stHeader.byLength		= *pbData++;
        out.stHeader.wHandle		= *(WORD*)pbData,pbData++,pbData++;

        //type specific data
        //START formatted section
        out.byVendor				= *pbData++;
        out.byBiosVersion			= *pbData++;
        out.wBIOSStartingSegment	= *(WORD*)pbData,pbData++,pbData++;

        out.byBIOSReleaseDate		= *pbData++;
        out.byBIOSROMSize			= *pbData++;

        for(int j = 0 ; j < 8 ; j++)
            out.qwBIOSCharacteristics[j] = *pbData++;

        out.byExtensionByte1		= *pbData++;
        out.byExtensionByte2		= *pbData++;
        out.bySystemBIOSMajorRelease = *pbData++;
        out.bySystemBIOSMinorRelease = *pbData++;
        out.byEmbeddedFirmwareMajorRelease = *pbData++;
        out.byEmbeddedFirmwareMinorRelease = *pbData++;
        //END formatted section

        //START unformatted section
        pbSectionStart += out.stHeader.byLength;

        out.szVendor				= GetString(pbSectionStart,out.byVendor);
        out.szBIOSVersion			= GetString(pbSectionStart,out.byBiosVersion);
        out.szBIOSReleaseDate		= GetString(pbSectionStart,out.byBIOSReleaseDate);
        //END unformatted section
    }
    return (pbData != NULL);
}

////////////////////////////////////////////////////////////////////////////////

//#### System Information ####
BOOL SMBIOS::GetData(SMB_TYPE1& out)
{
    DWORD dwTableSize;
    BYTE* pbData = FetchNext(NULL, FALSE, 1, dwTableSize);
    BYTE* pbSectionStart = pbData;

    memset(&out, 0 ,sizeof(out));

    if(pbData)
    {
        if(m_byMajorVersion >=2 && m_byMinorVersion > 0)
        {
            //mandatory header
            out.stHeader.bySection	= *pbData++;
            out.stHeader.byLength	= *pbData++;
            out.stHeader.wHandle	= *(WORD*)pbData, pbData++, pbData++;

            //type specific data
            //START formatted section
            out.byManufacturer		= *pbData++;
            out.byProductName		= *pbData++;
            out.byVersion			= *pbData++;
            out.bySerialNumber		= *pbData++;
        }

        if(m_byMajorVersion >=2 && m_byMinorVersion > 1)
        {
            for(int j = 0 ; j < 16 ; j++)
                out.byUUID[j]		= *pbData++;
            out.byWakeupType		= *pbData++;
        }

        if(m_byMajorVersion >=2 && m_byMinorVersion > 4)
        {
            out.bySKUNumber			= *pbData++;
            out.byFamily			= *pbData++;
        }
        //END formatted section

        //START unformatted section
        pbSectionStart += out.stHeader.byLength;

        if(out.byManufacturer)
            out.szManufacturer		= GetString(pbSectionStart, out.byManufacturer);
        if(out.byProductName)
            out.szProductName		= GetString(pbSectionStart, out.byProductName);
        if(out.byVersion)
            out.szVersion			= GetString(pbSectionStart, out.byVersion);
        if(out.bySerialNumber)
            out.szSerialNumber		= GetString(pbSectionStart, out.bySerialNumber);
        if(out.bySKUNumber)
            out.szSKUNumber			= GetString(pbSectionStart, out.bySKUNumber);
        if(out.byFamily)
            out.szFamily			= GetString(pbSectionStart, out.byFamily);
        //END unformatted section
    }
    return (pbData != NULL);
}

////////////////////////////////////////////////////////////////////////////////

//#### Base Board Information ####
BOOL SMBIOS::GetData(SMB_TYPE2& out)
{
    DWORD dwTableSize;
    BYTE* pbData = FetchNext(NULL, FALSE, 2, dwTableSize);
    BYTE* pbSectionStart = pbData;

    memset(&out, 0, sizeof(out));

    if(pbData)
    {
        //mandatory header
        out.stHeader.bySection		= *pbData++;
        out.stHeader.byLength		= *pbData++;
        out.stHeader.wHandle		= *(WORD*)pbData,pbData++,pbData++;

        //type specific data
        //START formatted section
        out.byManufacturer			= *pbData++;
        out.byProductName			= *pbData++;
        out.byVersion				= *pbData++;
        out.bySerialNumber			= *pbData++;
        out.byAssetTag				= *pbData++;
        out.byFeatureFlags			= *pbData++;
        out.byLocationInChassis		= *pbData++;
        out.wChassisHandle			= *(WORD*)pbData,pbData++,pbData++;
        out.byBoardType				= *pbData++;
        out.byNoOfContainedObjectHandles = *pbData++;

        for(int j = 0 ; j < out.byNoOfContainedObjectHandles ; j++)
            out.ContainedObjectHandles[j] = (WORD*)pbData,pbData++,pbData++;
        //END formatted section

        //START unformatted section
        pbSectionStart += out.stHeader.byLength;

        if(out.byManufacturer)
            out.szManufacturer		= GetString(pbSectionStart,out.byManufacturer);
        if(out.byProductName)
            out.szProductName		= GetString(pbSectionStart,out.byProductName);
        if(out.byVersion)
            out.szVersion			= GetString(pbSectionStart,out.byVersion);
        if(out.bySerialNumber)
            out.szSerialNumber		= GetString(pbSectionStart,out.bySerialNumber);
        if(out.byAssetTag)
            out.szAssetTag			= GetString(pbSectionStart,out.byAssetTag);
        if(out.byLocationInChassis)
            out.szLocationInChassis = GetString(pbSectionStart,out.byLocationInChassis);
        //END unformatted section
    }
    return (pbData != NULL);
}

////////////////////////////////////////////////////////////////////////////////

//#### System Enclosure or Chassis Type ####
BOOL SMBIOS::GetData(SMB_TYPE3& out)
{
    DWORD dwTableSize;
    BYTE* pbData = FetchNext(NULL,FALSE,3,dwTableSize);
    BYTE* pbSectionStart = pbData;

    memset(&out, 0, sizeof(out));

    if(pbData)
    {
        if(m_byMajorVersion >=2 && m_byMinorVersion > 0)
        {
            //mandatory header
            out.stHeader.bySection	= *pbData++;
            out.stHeader.byLength	= *pbData++;
            out.stHeader.wHandle	= *(WORD*)pbData,pbData++,pbData++;

            //type specific data
            //START formatted section
            out.byManufacturer		= *pbData++;
            out.byType				= *pbData++;
            out.byVersion			= *pbData++;
            out.bySerialNumber		= *pbData++;
            out.byAssetTag			= *pbData++;
        }

        if(m_byMajorVersion >=2 && m_byMinorVersion > 1)
        {
            out.byBootupState		= *pbData++;
            out.byPowerSupplyState	= *pbData++;
            out.byThermalState		= *pbData++;
            out.bySecurityStatus	= *pbData++;
        }

        if(m_byMajorVersion >=2 && m_byMinorVersion > 3)
        {
            out.dwOEMdefined		= *(DWORD*)pbData,pbData++,pbData++,pbData++,pbData++;
            out.byHeight			= *pbData++;
            out.byNumberOfPowerCords = *pbData++;
            out.byContainedElementCount = *pbData++;
            out.byContainedElementRecordLength = *pbData++;
        }

        //END formatted section

        //START unformatted section
        pbSectionStart += out.stHeader.byLength;

        if(out.byManufacturer)
            out.szManufacturer		= GetString(pbSectionStart,out.byManufacturer);
        if(out.byVersion)
            out.szVersion			= GetString(pbSectionStart,out.byVersion);
        if(out.bySerialNumber)
            out.szSerialNumber		= GetString(pbSectionStart,out.bySerialNumber);
        if(out.byAssetTag)
            out.szAssetTag			= GetString(pbSectionStart,out.byAssetTag);
        //END unformatted section
    }
    return (pbData != NULL);
}

////////////////////////////////////////////////////////////////////////////////

//#### Processor Information ####
BOOL SMBIOS::GetData(SMB_TYPE4& out)
{
    DWORD dwTableSize;
    BYTE* pbData = FetchNext(NULL, FALSE, 4, dwTableSize);
    BYTE* pbSectionStart = pbData;

    memset(&out, 0, sizeof(out));

    if(pbData)
    {
        if(m_byMajorVersion >=2 && m_byMinorVersion > 0)
        {
            //mandatory header
            out.stHeader.bySection	= *pbData++;
            out.stHeader.byLength	= *pbData++;
            out.stHeader.wHandle	= *(WORD*)pbData,pbData++,pbData++;

            //type specific data
            //START formatted section
            out.bySocketDesignation = *pbData++;
            out.byProcessorType		= *pbData++;
            out.byProcessorFamily	= *pbData++;
            out.byProcessorManufacturer = *pbData++;

            for(int j = 0 ; j < 8; j++)
                out.qwProcessorID[j] = *pbData++;

            out.byProcessorVersion	= *pbData++;
            out.byVoltage			= *pbData++;

            out.wExternalClock		= *pbData,pbData++,pbData++;
            out.wMaxSpeed			= *pbData,pbData++,pbData++;
            out.wCurrentSpeed		= *pbData,pbData++,pbData++;

            out.byStatus			= *pbData++;
            out.byProcessorUpgrade	= *pbData++;
        }

        if(m_byMajorVersion >=2 && m_byMinorVersion > 1)
        {
            out.wL1CacheHandle		= *(WORD*)pbData,pbData++,pbData++;
            out.wL2CacheHandle		= *(WORD*)pbData,pbData++,pbData++;
            out.wL3CacheHandle		= *(WORD*)pbData,pbData++,pbData++;
        }

        if(m_byMajorVersion >=2 && m_byMinorVersion > 3)
        {
            out.bySerialNumber		= *pbData++;
            out.byAssetTagNumber	= *pbData++;
            out.byPartNumber		= *pbData++;
        }

        //END formatted section

        //START unformatted section
        pbSectionStart += out.stHeader.byLength;

        if(out.bySocketDesignation)
            out.szSocketDesignation = GetString(pbSectionStart,out.bySocketDesignation);
        if(out.byProcessorManufacturer)
            out.szProcessorManufacturer = GetString(pbSectionStart,out.byProcessorManufacturer);
        if(out.bySerialNumber)
            out.szSerialNumber		= GetString(pbSectionStart,out.bySerialNumber);
        if(out.byAssetTagNumber)
            out.szAssetTagNumber	= GetString(pbSectionStart,out.byAssetTagNumber);
        if(out.byPartNumber)
            out.szPartNumber		= GetString(pbSectionStart,out.byPartNumber);
        //END unformatted section
    }
    return (pbData != NULL);
}

////////////////////////////////////////////////////////////////////////////////
