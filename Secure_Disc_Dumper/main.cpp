#include <windows.h>
#include <iostream>
#include <fstream>
#include <conio.h>
#include <string>
#include <iomanip>

using namespace std;

#define SECTOR_SIZE 512
#define READ_BLOCK_SIZE SECTOR_SIZE*512

#define MAX_DISK_NUM 32

uint8_t buf1[READ_BLOCK_SIZE];
uint8_t buf2[READ_BLOCK_SIZE];

#define DRIVENAME L"\\\\.\\PhysicalDrive"

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

    return bResult;
}

class DriveInfo
{
public: 
    LPWSTR DriveName = L"";
    uint64_t SectorNum = 0;
};

void main()
{
    DriveInfo drives[MAX_DISK_NUM];

    string filename = "dump.img";

    DISK_GEOMETRY_EX pdg = { 0 };
    
    uint32_t index = 0;
    uint32_t filledindex = 0;

    uint64_t sectors_num = 0;

    for (uint32_t i = 0; i < MAX_DISK_NUM; i++)
    {
        LPWSTR tempname = concat(DRIVENAME, to_string(i));
        if (!GetDriveGeometry(tempname, &pdg))
            break;
        drives[filledindex].DriveName = tempname;
        drives[filledindex].SectorNum = pdg.Geometry.Cylinders.QuadPart *  pdg.Geometry.TracksPerCylinder * pdg.Geometry.SectorsPerTrack;
        cout << filledindex << "\t";
        wcout << drives[filledindex].DriveName;
        cout << "\tSectors: " << drives[filledindex].SectorNum << endl;
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
    cout << "Enter sector number to read (0 for auto full drive): ";
    cin >> sectors_num;
    if (sectors_num == 0)
        sectors_num = drives[index].SectorNum;
    cout << endl <<"Press any key to start." << endl;
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

        if (!arrays_equal(buf1, buf2, READ_BLOCK_SIZE))
        {
            cout
                << "Mismatch!" << "\t"
                << "Sector: " << i << "\t"
                << "Address: " << hex << ptr_curr.QuadPart << endl;
                
            SetFilePointerEx(hDisk, ptr_shift_inv, 0, FILE_CURRENT);
            error_count++;
            continue;
        }

        outFile.write((char*)buf1, READ_BLOCK_SIZE);

        if (i % (SECTOR_SIZE*32) == 0)
        {
            double percent = i * 100.0 / sectors_num;
            cout
                << internal << left << setfill('0') << setw(7) << percent << " %\t"
                << "Sector: " << i << "\t"
                << "Address: " << hex << ptr_curr.QuadPart << endl;
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
