
#include <windows.h>
#include <errno.h>
#include <io.h>
#include <psapi.h>

#include "mman.h"

#ifndef FILE_MAP_EXECUTE
#define FILE_MAP_EXECUTE    0x0020
#endif /* FILE_MAP_EXECUTE */

static int __map_mman_error(DWORD err)
{
    // Unwrap FACILITY_WIN32 HRESULT errors.
    if ((err & 0xFFFF0000) == 0x80070000)
    {
        err &= 0x0000FFFF;
    }

    switch (err)
    {
        case ERROR_FILE_NOT_FOUND:       //    2
        case ERROR_PATH_NOT_FOUND:       //    3
        case ERROR_INVALID_DRIVE:        //   15
        case ERROR_NO_MORE_FILES:        //   18
        case ERROR_BAD_NETPATH:          //   53
        case ERROR_BAD_NET_NAME:         //   67
        case ERROR_BAD_PATHNAME:         //  161
        case ERROR_FILENAME_EXCED_RANGE: //  206
            return ENOENT;

        case ERROR_BAD_ENVIRONMENT: //   10
            return E2BIG;

        case ERROR_BAD_FORMAT:                //   11
        case ERROR_INVALID_STARTING_CODESEG:  //  188
        case ERROR_INVALID_STACKSEG:          //  189
        case ERROR_INVALID_MODULETYPE:        //  190
        case ERROR_INVALID_EXE_SIGNATURE:     //  191
        case ERROR_EXE_MARKED_INVALID:        //  192
        case ERROR_BAD_EXE_FORMAT:            //  193
        case ERROR_ITERATED_DATA_EXCEEDS_64k: //  194
        case ERROR_INVALID_MINALLOCSIZE:      //  195
        case ERROR_DYNLINK_FROM_INVALID_RING: //  196
        case ERROR_IOPL_NOT_ENABLED:          //  197
        case ERROR_INVALID_SEGDPL:            //  198
        case ERROR_AUTODATASEG_EXCEEDS_64k:   //  199
        case ERROR_RING2SEG_MUST_BE_MOVABLE:  //  200
        case ERROR_RELOC_CHAIN_XEEDS_SEGLIM:  //  201
        case ERROR_INFLOOP_IN_RELOC_CHAIN:    //  202
            return ENOEXEC;

        case ERROR_INVALID_HANDLE:        //    6
        case ERROR_INVALID_TARGET_HANDLE: //  114
        case ERROR_DIRECT_ACCESS_HANDLE:  //  130
            return EBADF;

        case ERROR_WAIT_NO_CHILDREN:   //  128
        case ERROR_CHILD_NOT_COMPLETE: //  129
            return ECHILD;

        case ERROR_NO_PROC_SLOTS:       //   89
        case ERROR_MAX_THRDS_REACHED:   //  164
        case ERROR_NESTING_NOT_ALLOWED: //  215
            return EAGAIN;

        case ERROR_ARENA_TRASHED:     //    7
        case ERROR_NOT_ENOUGH_MEMORY: //    8
        case ERROR_INVALID_BLOCK:     //    9
        case ERROR_NOT_ENOUGH_QUOTA:  // 1816
            return ENOMEM;

        case ERROR_ACCESS_DENIED:           //    5
        case ERROR_CURRENT_DIRECTORY:       //   16
        case ERROR_WRITE_PROTECT:           //   19
        case ERROR_BAD_UNIT:                //   20
        case ERROR_NOT_READY:               //   21
        case ERROR_BAD_COMMAND:             //   22
        case ERROR_CRC:                     //   23
        case ERROR_BAD_LENGTH:              //   24
        case ERROR_SEEK:                    //   25
        case ERROR_NOT_DOS_DISK:            //   26
        case ERROR_SECTOR_NOT_FOUND:        //   27
        case ERROR_OUT_OF_PAPER:            //   28
        case ERROR_WRITE_FAULT:             //   29
        case ERROR_READ_FAULT:              //   30
        case ERROR_GEN_FAILURE:             //   31
        case ERROR_SHARING_VIOLATION:       //   32
        case ERROR_LOCK_VIOLATION:          //   33
        case ERROR_WRONG_DISK:              //   34
        case ERROR_SHARING_BUFFER_EXCEEDED: //   36
        case ERROR_NETWORK_ACCESS_DENIED:   //   65
        case ERROR_CANNOT_MAKE:             //   82
        case ERROR_FAIL_I24:                //   83
        case ERROR_DRIVE_LOCKED:            //  108
        case ERROR_SEEK_ON_DEVICE:          //  132
        case ERROR_NOT_LOCKED:              //  158
        case ERROR_LOCK_FAILED:             //  167
        case 35:                            //   35 (undefined)
            return EACCES;

        case ERROR_FILE_EXISTS:    //   80
        case ERROR_ALREADY_EXISTS: //  183
            return EEXIST;

        case ERROR_NOT_SAME_DEVICE: //   17
            return EXDEV;

        case ERROR_DIRECTORY: //  267 (bpo-12802)
            return ENOTDIR;

        case ERROR_TOO_MANY_OPEN_FILES: //    4
            return EMFILE;

        case ERROR_DISK_FULL: //  112
            return ENOSPC;

        case ERROR_BROKEN_PIPE: //  109
        case ERROR_NO_DATA:     //  232 (bpo-13063)
            return EPIPE;

        case ERROR_DIR_NOT_EMPTY: //  145
            return ENOTEMPTY;

        case ERROR_NO_UNICODE_TRANSLATION: // 1113
            return EILSEQ;

        case ERROR_INVALID_FUNCTION:  //    1
        case ERROR_INVALID_ACCESS:    //   12
        case ERROR_INVALID_DATA:      //   13
        case ERROR_INVALID_PARAMETER: //   87
        case ERROR_NEGATIVE_SEEK:     //  131
        case ERROR_USER_MAPPED_FILE:  //  1224
            return EINVAL;
        default:
            return -1;
    }
}

static DWORD __map_mmap_prot_page(const int prot)
{
    DWORD protect = 0;
    
    if (prot == PROT_NONE)
        return protect;
        
    if ((prot & PROT_EXEC) != 0)
    {
        protect = ((prot & PROT_WRITE) != 0) ? 
                    PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    }
    else
    {
        protect = ((prot & PROT_WRITE) != 0) ?
                    PAGE_READWRITE : PAGE_READONLY;
    }
    
    return protect;
}

static DWORD __map_mmap_prot_file(const int prot)
{
    DWORD desiredAccess = 0;
    
    if (prot == PROT_NONE)
        return desiredAccess;
        
    if ((prot & PROT_READ) != 0)
        desiredAccess |= FILE_MAP_READ;
    if ((prot & PROT_WRITE) != 0)
        desiredAccess |= FILE_MAP_WRITE;
    if ((prot & PROT_EXEC) != 0)
        desiredAccess |= FILE_MAP_EXECUTE;
    
    return desiredAccess;
}

void* mmap(void *addr, size_t len, int prot, int flags, int fildes, OffsetType off)
{
    HANDLE fm, h;
    
    void * map = MAP_FAILED;
    
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4293)
#endif

    const DWORD dwFileOffsetLow = (sizeof(OffsetType) <= sizeof(DWORD)) ?
                    (DWORD)off : (DWORD)(off & 0xFFFFFFFFL);
    const DWORD dwFileOffsetHigh = (sizeof(OffsetType) <= sizeof(DWORD)) ?
                    (DWORD)0 : (DWORD)((off >> 32) & 0xFFFFFFFFL);
    const DWORD protect = __map_mmap_prot_page(prot);
    const DWORD desiredAccess = __map_mmap_prot_file(prot);

    const OffsetType maxSize = off + (OffsetType)len;

    const DWORD dwMaxSizeLow = (sizeof(OffsetType) <= sizeof(DWORD)) ?
                    (DWORD)maxSize : (DWORD)(maxSize & 0xFFFFFFFFL);
    const DWORD dwMaxSizeHigh = (sizeof(OffsetType) <= sizeof(DWORD)) ?
                    (DWORD)0 : (DWORD)((maxSize >> 32) & 0xFFFFFFFFL);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    errno = 0;
    
    if (len == 0 
        /* Usupported protection combinations */
        || prot == PROT_EXEC)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }
    
    h = ((flags & MAP_ANONYMOUS) == 0) ? 
                    (HANDLE)_get_osfhandle(fildes) : INVALID_HANDLE_VALUE;

    if ((flags & MAP_ANONYMOUS) == 0 && h == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return MAP_FAILED;
    }

    fm = CreateFileMapping(h, NULL, protect, dwMaxSizeHigh, dwMaxSizeLow, NULL);

    if (fm == NULL)
    {
        errno = __map_mman_error(GetLastError());
        return MAP_FAILED;
    }
  
    if ((flags & MAP_FIXED) == 0)
    {
        map = MapViewOfFile(fm, desiredAccess, dwFileOffsetHigh, dwFileOffsetLow, len);
    }
    else
    {
        map = MapViewOfFileEx(fm, desiredAccess, dwFileOffsetHigh, dwFileOffsetLow, len, addr);
    }

    CloseHandle(fm);
  
    if (map == NULL)
    {
        errno = __map_mman_error(GetLastError());
        return MAP_FAILED;
    }

    return map;
}

int munmap(void *addr, size_t len)
{
    if (UnmapViewOfFile(addr))
        return 0;
        
    errno =  __map_mman_error(GetLastError());
    
    return -1;
}

int _mprotect(void *addr, size_t len, int prot)
{
    DWORD newProtect = __map_mmap_prot_page(prot);
    DWORD oldProtect = 0;
    
    if (VirtualProtect(addr, len, newProtect, &oldProtect))
        return 0;
    
    errno =  __map_mman_error(GetLastError());
    
    return -1;
}

int msync(void *addr, size_t len, int flags)
{
    if (FlushViewOfFile(addr, len))
        return 0;
    
    errno =  __map_mman_error(GetLastError());
    
    return -1;
}

int mlock(const void *addr, size_t len)
{
    if (VirtualLock((LPVOID)addr, len))
        return 0;
        
    errno =  __map_mman_error(GetLastError());
    
    return -1;
}

int munlock(const void *addr, size_t len)
{
    if (VirtualUnlock((LPVOID)addr, len))
        return 0;
        
    errno =  __map_mman_error(GetLastError());
    
    return -1;
}

static int __madvise_willneed(void* addr, size_t size)
{
    WIN32_MEMORY_RANGE_ENTRY region = {};
    region.VirtualAddress = addr;
    region.NumberOfBytes = size;
    WINBOOL success = PrefetchVirtualMemory(GetCurrentProcess(), 
                                            1,
                                            &region,
                                            0);

    if(success == 0)
    {
        errno = __map_mman_error(GetLastError());
        return -1;
    }

    return 0;
}

static int __madvise_dontneed(void* addr, size_t size)
{
    SYSTEM_INFO sysinfo = {};
    GetSystemInfo(&sysinfo);

    // Must be page aligned
    if((uintptr_t)addr % sysinfo.dwPageSize != 0 || size % sysinfo.dwPageSize != 0)
    {
        errno = EINVAL;
        return -1;
    }

    // check if there is a page in the region that is locked
    PSAPI_WORKING_SET_EX_INFORMATION wset_info = {.VirtualAddress = addr};
    WINBOOL locked = 0;

    while((uintptr_t)wset_info.VirtualAddress < (uintptr_t)addr + size - sysinfo.dwPageSize &&
          locked == 0)
    {
        if(QueryWorkingSetEx(GetCurrentProcess(), &wset_info, sizeof(wset_info)) == 0)
        {
            errno = __map_mman_error(GetLastError());
            return -1;
        }

        locked = wset_info.VirtualAttributes.Valid && wset_info.VirtualAttributes.Locked;
        wset_info.VirtualAddress = (PVOID)((uintptr_t)wset_info.VirtualAddress + sysinfo.dwPageSize);
    }

    if(locked == 1)
    {
        errno = EINVAL;
        return -1;
    }

    DWORD err = OfferVirtualMemory(addr, size, VmOfferPriorityBelowNormal);
    if(err != ERROR_SUCCESS)
    {
        errno = __map_mman_error(err);
        return -1;
    }

    return 0;
}

int madvise(void* addr, size_t size, int advice)
{
    errno = 0;
    switch(advice)
    {
        case MADV_WILLNEED:
            return __madvise_willneed(addr, size);
        case MADV_DONTNEED: // OfferVirtualMemory will cause addressing error
            return __madvise_dontneed(addr, size);
        // Below has no equivalents under Windows (I have not yet found any)
        case MADV_NORMAL:
        case MADV_RANDOM:
        case MADV_SEQUENTIAL:
        default:
            return 0;
    }
}
