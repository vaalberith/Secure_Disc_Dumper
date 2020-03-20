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

#define DRIVENAME L"\\\\.\\PhysicalDrive4"

LPCWSTR drive = DRIVENAME;
LPWSTR driveI = DRIVENAME;

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

void GetDriveGeometry(LPWSTR wszPath, DISK_GEOMETRY *pdg)
{
    HANDLE hDevice = INVALID_HANDLE_VALUE;  // handle to the drive to be examined 
    DWORD junk = 0;                     // discard results

    hDevice = CreateFileW(wszPath,          // drive to open
        0,                // no access to the drive
        FILE_SHARE_READ | // share mode
        FILE_SHARE_WRITE,
        NULL,             // default security attributes
        OPEN_EXISTING,    // disposition
        0,                // file attributes
        NULL);            // do not copy file attributes

    DeviceIoControl(hDevice,                       // device to be queried
        IOCTL_DISK_GET_DRIVE_GEOMETRY, // operation to perform
        NULL, 0,                       // no input buffer
        pdg, sizeof(*pdg),            // output buffer
        &junk,                         // # bytes returned
        (LPOVERLAPPED)NULL);          // synchronous I/O

    CloseHandle(hDevice);
}

void main()
{
    uint32_t block_num = 15597568;

    //DISK_GEOMETRY pdg = { 0 };
    //GetDriveGeometry(driveI, &pdg);

    //block_num = (ULONG)pdg.Cylinders.QuadPart * (ULONG)pdg.TracksPerCylinder * (ULONG)pdg.SectorsPerTrack;

    string filename = "dump.img";

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
