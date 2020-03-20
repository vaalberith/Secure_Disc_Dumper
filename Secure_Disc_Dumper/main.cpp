#include <windows.h>
#include <iostream>
#include <fstream>
#include <conio.h>
#include <string>
#include <iomanip>

using namespace std;

#define BLOCK_SIZE 512


uint8_t buf1[BLOCK_SIZE];
uint8_t buf2[BLOCK_SIZE];
uint8_t buf3[BLOCK_SIZE];

#define DRIVENAME L"\\\\.\\PhysicalDrive"

LPWSTR drive = DRIVENAME;

uint32_t error_count = 0;

bool arrays_equal(uint8_t *ptr1, uint8_t *ptr2, uint32_t len)
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

bool GetDriveGeometry(LPWSTR drivepath, DISK_GEOMETRY_EX *pdg)
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

    CloseHandle(hDevice);
    
    /*ULARGE_INTEGER ulFreeSpace, ulTotalSpace, ulTotalFreeSpace;
    bResult = GetDiskFreeSpaceEx(drivepath, &ulFreeSpace, &ulTotalSpace, &ulTotalFreeSpace);*/

    return bResult;
}

void main()
{
    uint32_t block_num = 15597568;

    DISK_GEOMETRY_EX pdg = { 0 };
    uint64_t real_sector_num = 0;
    string filename = "dump.img";
    uint32_t index = 0;

    //block_num = (ULONG)pdg.Cylinders.QuadPart * (ULONG)pdg.TracksPerCylinder * (ULONG)pdg.SectorsPerTrack;

    for (uint32_t i = 0; i < 32; i++)
    {
        drive = concat(DRIVENAME, to_string(i));
        if (!GetDriveGeometry(drive , &pdg))
            break;
        wcout << drive;
        cout << "\tSectors: " << pdg.Geometry.Cylinders.QuadPart *  pdg.Geometry.TracksPerCylinder * pdg.Geometry.SectorsPerTrack << endl;
    }
    cout << "Enter disk index: ";
    
    cin >> index;
    cout << endl << "Selected drive: ";
    drive = concat(DRIVENAME, to_string(index));
    wcout << drive << endl;
    
    cout << "Press any key to start." << endl;

    _getch();

    ofstream outFile(filename, ofstream::binary);

    HANDLE hDisk = CreateFile(drive, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    LARGE_INTEGER ptr_shift_0 = { 0 };
    LARGE_INTEGER ptr_shift_inv = { 0 };
    ptr_shift_inv.QuadPart = -BLOCK_SIZE;
    LARGE_INTEGER ptr_curr = { 0 };

    SetFilePointerEx(hDisk, ptr_shift_0, &ptr_curr, FILE_BEGIN);

    for (uint32_t i = 0; i < block_num; i++)
    {
        ReadFile(hDisk, buf1, BLOCK_SIZE, 0, NULL);
        SetFilePointerEx(hDisk, ptr_shift_inv, 0, FILE_CURRENT);
        ReadFile(hDisk, buf2, BLOCK_SIZE, 0, NULL);

        if (!arrays_equal(buf1, buf2, BLOCK_SIZE))
        {
            cout
                << "Mismatch!" << "\t"
                << "Addr: " << hex << ptr_curr.QuadPart << "\t"
                << "Sector: " << i << endl;
            i--;
            SetFilePointerEx(hDisk, ptr_shift_inv, 0, FILE_CURRENT);
            error_count++;
            Sleep(100);
            continue;
        }

        outFile.write((char*)buf1, BLOCK_SIZE);

        if (i % 1024 == 0)
        {
            double percent = i * 100.0 / block_num;
            cout
                << internal << left << setfill('0') << setw(10) << percent << " %\t"
                << "Sector: " << i << "\t"
                << "Address: " << hex << ptr_curr.QuadPart << endl;
        }


        SetFilePointerEx(hDisk, ptr_shift_0, &ptr_curr, FILE_CURRENT);
    }

    outFile.close();
    CloseHandle(hDisk);

    cout
        << "Finished!" << endl
        << "Errors number: " << error_count << endl;

    _getch();
}
