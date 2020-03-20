#include <windows.h>
#include <iostream>
#include <fstream>
#include <conio.h>
#include <string>
#include <iomanip>

#include "resource.h"

using namespace std;

#define SECTOR_SIZE 512
#define READ_BLOCK_SIZE SECTOR_SIZE*512

#define MAX_DISK_NUM 32

uint8_t buf1[READ_BLOCK_SIZE];
uint8_t buf2[READ_BLOCK_SIZE];

#define DRIVENAME L"\\\\.\\PhysicalDrive"

uint32_t error_count = 0;

bool arrays_equal_32(uint32_t *ptr1, uint32_t *ptr2, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        if (ptr1[i] != ptr2[i])
            return false;
    }
    return true;
}

LPWSTR concat(LPWSTR str1, string str2)
{
    LPWSTR str2lpwstr = new wchar_t[str2.size() + 1];
    for (uint32_t i = 0; i < str2.size(); i++)
        str2lpwstr[i] = str2[i];
    str2lpwstr[str2.size()] = 0;

    wstring w1(str1);
    wstring w2(str2lpwstr);
    wstring w3 = w1 + w2;

    LPWSTR result = new wchar_t[w3.size() + 1];
    for (uint32_t i = 0; i < w3.size(); i++)
        result[i] = w3[i];
    result[w3.size()] = 0;

    return result;
}

bool GetDriveInfo(LPWSTR drivepath, DISK_GEOMETRY_EX* pdg, string* productID)
{
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    bool bResult = false;

    hDevice = CreateFileW(drivepath,
        0,
        FILE_SHARE_READ |
        FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    bResult = DeviceIoControl(hDevice,
        IOCTL_DISK_GET_DRIVE_GEOMETRY,
        NULL, 0,
        pdg, sizeof(*pdg),
        0,
        NULL);


    STORAGE_PROPERTY_QUERY storagePropertyQuery;
    ZeroMemory(&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY));
    storagePropertyQuery.PropertyId = StorageDeviceProperty;
    storagePropertyQuery.QueryType = PropertyStandardQuery;

    STORAGE_DESCRIPTOR_HEADER storageDescriptorHeader = { 0 };

    DeviceIoControl(hDevice, 
        IOCTL_STORAGE_QUERY_PROPERTY,
        &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
        &storageDescriptorHeader, sizeof(STORAGE_DESCRIPTOR_HEADER),
        0, 
        NULL);


    const DWORD dwOutBufferSize = storageDescriptorHeader.Size;
    BYTE* pOutBuffer = new BYTE[storageDescriptorHeader.Size];
    ZeroMemory(pOutBuffer, storageDescriptorHeader.Size);


    DeviceIoControl(hDevice, 
        IOCTL_STORAGE_QUERY_PROPERTY,
        &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
        pOutBuffer, dwOutBufferSize,
        0, 
        NULL);

    STORAGE_DEVICE_DESCRIPTOR* pDeviceDescriptor = (STORAGE_DEVICE_DESCRIPTOR*)pOutBuffer;

    *productID = string((char*)(pOutBuffer + pDeviceDescriptor->ProductIdOffset));

    CloseHandle(hDevice);

    return bResult;
}

class DriveInfo
{
public: 
    LPWSTR DriveName = L"";
    uint64_t SectorNum = 0;
    string ProductID = "";
};

void main()
{
    DriveInfo drives[MAX_DISK_NUM];

    string filename = "dump.img";

    DISK_GEOMETRY_EX pdg = { 0 };
    string productID = "";
    
    uint32_t index = 0;
    uint32_t filledindex = 0;

    uint64_t sectors_num = 0;

    for (uint32_t i = 0; i < MAX_DISK_NUM; i++)
    {
        LPWSTR tempname = concat(DRIVENAME, to_string(i));

        if (!GetDriveInfo(tempname, &pdg, &productID))
            break;
        drives[filledindex].DriveName = tempname;
        drives[filledindex].ProductID = productID;
        drives[filledindex].SectorNum = pdg.Geometry.Cylinders.QuadPart *  pdg.Geometry.TracksPerCylinder * pdg.Geometry.SectorsPerTrack;
        cout << left << setw(5) << filledindex;
        wcout << left << setw(25) << drives[filledindex].DriveName;
        cout 
            << left << setw(30) << drives[filledindex].ProductID
            << "Sectors: " << drives[filledindex].SectorNum << endl;
        filledindex++;
    }
    
    bool err = false;
    do
    {
        err = false;
        cout << "Enter disk index: ";
        cin >> index;
        if (drives[index].DriveName == L"" || drives[index].SectorNum == 0)
        {
            err = true;
            cout << endl << "No such drive!" << endl;
        }
    } while (err);

    cout << endl << "Selected drive: ";
    wcout << drives[index].DriveName << endl;
    cout << "Enter sectors number to read (0 for auto full drive): ";
    cin >> sectors_num;
    if (sectors_num == 0)
        sectors_num = drives[index].SectorNum;
    cout
        << endl << "Sectors number will be read: "
        << sectors_num << endl
        << "Press any key to start." << endl;

    _getch();

    ofstream outFile(filename, ofstream::binary);

    HANDLE hDisk = CreateFile(drives[index].DriveName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    LARGE_INTEGER ptr_shift_0 = { 0 };
    LARGE_INTEGER ptr_shift_inv = { 0 };
    ptr_shift_inv.QuadPart = -READ_BLOCK_SIZE;
    LARGE_INTEGER ptr_curr = { 0 };

    SetFilePointerEx(hDisk, ptr_shift_0, &ptr_curr, FILE_BEGIN);

    for (uint32_t i = 0; i < sectors_num;)
    {
        ReadFile(hDisk, buf1, READ_BLOCK_SIZE, 0, NULL);
        SetFilePointerEx(hDisk, ptr_shift_inv, 0, FILE_CURRENT);
        ReadFile(hDisk, buf2, READ_BLOCK_SIZE, 0, NULL);

        if (!arrays_equal_32((uint32_t*)buf1, (uint32_t*)buf2, READ_BLOCK_SIZE/sizeof(uint32_t)))
        {
            cout
                << "Mismatch!\t"
                << "Sector: " << left << setw(15) << i
                << "Address: " << left << hex << ptr_curr.QuadPart << endl;
                
            SetFilePointerEx(hDisk, ptr_shift_inv, 0, FILE_CURRENT);
            error_count++;
            continue;
        }

        outFile.write((char*)buf1, READ_BLOCK_SIZE);

        if (i % (SECTOR_SIZE*32) == 0)
        {
            double percent = i * 100.0 / sectors_num;
            cout
                << right << setw(8) << fixed << setprecision(4) << percent << "%\t"
                << "Sector: " << left << setw(15) << i
                << "Address: " << left << hex << ptr_curr.QuadPart << endl;
        }


        SetFilePointerEx(hDisk, ptr_shift_0, &ptr_curr, FILE_CURRENT);
        i += READ_BLOCK_SIZE / SECTOR_SIZE;
    }

    outFile.close();
    CloseHandle(hDisk);

    cout
        << "Finished!" << endl
        << "Errors number: " << error_count << endl;

    _getch();
}
