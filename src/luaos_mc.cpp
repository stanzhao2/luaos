

#include <stdio.h>
#include <string>
#include <stdint.h>
#include <md5.h>

#ifdef _MSC_VER

#include "luaos_mc.h"
#include "bios/bios.h"

static bool run_wmi_service()
{
  SC_HANDLE hSC = OpenSCManagerA(NULL, NULL, GENERIC_EXECUTE);
  if (hSC == NULL)
    return false;

  SC_HANDLE hSvc = OpenServiceA(hSC, "Winmgmt", SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_STOP);
  if(hSvc == NULL)
  {
    CloseServiceHandle(hSC);
    return false;
  }
  SERVICE_STATUS status;
  if (QueryServiceStatus(hSvc, &status) == FALSE)
  {
    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSC);
    return false;
  }
  bool result = true;
  if (status.dwCurrentState == SERVICE_STOPPED)
  {
    if (StartServiceA(hSvc, NULL, NULL) == FALSE)
    {
      CloseServiceHandle(hSvc);
      CloseServiceHandle(hSC);
      return false;
    }
    while (QueryServiceStatus(hSvc, &status) == TRUE)
    {
      Sleep( status.dwWaitHint);
      if(status.dwCurrentState == SERVICE_RUNNING)
        break;
    }
  }
  CloseServiceHandle(hSvc);
  CloseServiceHandle(hSC);
  return result;
}

static std::string get_hw_code()
{
  SMBIOS bios;
  if (!bios.Init())
  {
    if (!run_wmi_service() || !bios.Init())
      return std::string();
  }

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
  return std::move(data);
}

#else

static std::string get_hw_code()
{
  FILE *fd = 0;
  char data[8192];
  std::string hwcode;
  const std::string cmd("dmidecode baseborad processor | grep 'ID'");
  if (fd = popen(cmd.c_str(), "r")){
    while (fgets(data, sizeof(data), fd) != NULL) {
      hwcode += data;
    }
    pclose(fd);
  }
  return hwcode;
}

#endif

static unsigned int hash32(const void* key, size_t len)
{
  const unsigned int m = 0x5bd1e995;
  const int r = 24;
  const int seed = 97;
  unsigned int h = seed ^ (int)len;

  const unsigned char* data = (const unsigned char*)key;
  while (len >= 4) {
    unsigned int k = *(unsigned int*)data;
    k *= m;
    k ^= k >> r;
    k *= m;
    h *= m;
    h ^= k;
    data += 4;
    len -= 4;
  }
  switch (len) {
  case 3: h ^= data[2] << 16;
  case 2: h ^= data[1] << 8;
  case 1: h ^= data[0];
    h *= m;
  };
  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h;
}

int code_generate(lua_State* L)
{
  std::string code(get_hw_code());
  unsigned char hash[16];
  md5::hash(code.c_str(), code.size(), &hash);
  
  std::string result;
  for (int i = 0; i < 16; i++) {
    char hex[3];
    sprintf(hex, "%02x", hash[i]);
    result += hex;
  }
  lua_pushlstring(L, result.c_str(), result.size());
  return 1;
}

/***********************************************************************************/
