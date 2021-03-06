
#include "stdafx.h"
#include "xosc.h"

/*
    Reversed by: Medaka

    This is the entire XOSC module in C code.
    Please give credit where it's due, don't just copy and paste, please.
    I've worked really hard on this trying to perfect evey last bit of this.
*/

byte* xamAllocatedData;

extern "C" {
    NTSTATUS XamAlloc(int size, int size2, byte* data);
    void XamFree(PBYTE allocatedData);
    NTSTATUS IoSynchronousDeviceIoControlRequest(int io_request, void** deviceObject, void* bufferInput, int length, int r7, int r8, void* output);
    void HalReadWritePCISpace(int r3, int r4, int r5, int r6, void* buffer, int length, bool WritePCISpace);
}

NTSTATUS SysChall_GetStorageDeviceSize(char* path, int* outSize) {
    *outSize = 0;

    HANDLE file = 0;
    NTSTATUS status = STATUS_SUCCESS;

    ANSI_STRING deviceName = { 0 };
    OBJECT_ATTRIBUTES objAttr = { 0 };
    IO_STATUS_BLOCK statusBlock = { 0 };
    FILE_FS_SIZE_INFORMATION sizeInfo = { 0 };

    RtlInitAnsiString(&deviceName, path);
    InitializeObjectAttributes(&objAttr, &deviceName, OBJ_CASE_INSENSITIVE, 0);

    if (NT_SUCCESS(NtOpenFile(&file, (SYNCHRONIZE | 1), &objAttr, &statusBlock, FILE_SHARE_READ, XOSC_DEVICESIZE_OPEN_OPTIONS)))
        if (NT_SUCCESS(status = NtQueryVolumeInformationFile(file, &statusBlock, &sizeInfo, 0x18, FileFsSizeInformation)))
            *outSize = sizeInfo.TotalAllocationUnits.LowPart;

    NtClose(file);
    return status;
}

NTSTATUS SysChall_GetStorageDeviceSizes(xoscResponse* chalResp) {
    chalResp->operations |= 0x10;

    SysChall_GetStorageDeviceSize("\\Device\\Mu0\\", &chalResp->memoryUnit0);
    SysChall_GetStorageDeviceSize("\\Device\\Mu1\\", &chalResp->memoryUnit1);
    SysChall_GetStorageDeviceSize("\\Device\\BuiltInMuSfc\\", &chalResp->memoryUnitIntFlash);
    SysChall_GetStorageDeviceSize("\\Device\\BuiltInMuUsb\\Storage\\", &chalResp->memoryUnitIntUSB);
    SysChall_GetStorageDeviceSize("\\Device\\Mass0PartitionFile\\Storage\\", &chalResp->mass0PartitionFileSize);
    SysChall_GetStorageDeviceSize("\\Device\\Mass1PartitionFile\\Storage\\", &chalResp->mass1PartitionFileSize);
    SysChall_GetStorageDeviceSize("\\Device\\Mass2PartitionFile\\Storage\\", &chalResp->mass2PartitionFileSize);

    return STATUS_SUCCESS;
}

NTSTATUS SysChall_GetConsoleKVCertificate(xoscResponse* chalResp) {
    NTSTATUS keyStatus = STATUS_SUCCESS;
    WORD certificateDataLength = 0;
    XE_CONSOLE_CERTIFICATE certData = { 0 };

    if (NT_SUCCESS(keyStatus = XeKeysGetKey(XEKEY_XEIKA_CERTIFICATE, (PVOID)&certData, (PDWORD)&certificateDataLength))) {
        if (certificateDataLength > 0x110 || certData.signature[0x64] == 0x4F534947 || *(short*)certData.signature[0x68] >= 1) {
            chalResp->operations |= 2;
            memcpy((byte*)(chalResp + 0xB4), &certData.signature[0x6C], 0x24);
            *(int*)((int)chalResp + 0x80) = *(byte*)((int)&certData + 0x117);
        } else keyStatus = STATUS_INVALID_PARAMETER_1;
    }

    return chalResp->keyResultCert = keyStatus;
}

NTSTATUS SysChall_GetDeviceControlRequest(xoscResponse* chalResp) {
    chalResp->operations |= 1;

    PVOID deviceObject = NULL;
    BYTE partitionInfo[0x24] = { 0 };
    NTSTATUS status = STATUS_SUCCESS;
    STRING objectPath = { 0xE, 0xF, "\\Device\\Cdrom0" };

    if (!NT_SUCCESS(status = ObReferenceObjectByName(&objectPath, 0, 0, 0, &deviceObject)))
        return chalResp->ioCtlResult = status;

    *(long long*)((int)chalResp + 0xF0) = -1;
    *(long long*)((int)chalResp + 0xF8) = -1;
    *(long long*)((int)chalResp + 0x100) = -1;
    *(long long*)((int)chalResp + 0x108) = -1;
    *(long long*)((int)chalResp + 0x110) = -1;

    *(short*)((int)deviceObject + 0x20) = *(short*)((int)chalResp + 0xF0);

    *(char*)partitionInfo = 0x24;
    *(char*)(partitionInfo + 0x7) = 1;
    *(int*)(partitionInfo + 0x8) = 0x24;
    *(char*)(partitionInfo + 0x14) = 0x12;
    *(char*)(partitionInfo + 0x18) = 0x24;
    *(char*)(partitionInfo + 0x19) = 0xC0;

    status = IoSynchronousDeviceIoControlRequest(IOCTL_DISK_VERIFY, &deviceObject, partitionInfo, 0x24, 0, 0, 0);
    ObDereferenceObject(deviceObject);

    return chalResp->ioCtlResult = status;
}

NTSTATUS SysChall_SetupSataDiskHash(xoscResponse* chalResp) {
    XECRYPT_RSA rsa = { 0 };
    byte outDigest[0x14] = { 0 };
    int rsaSecuritySize = 0x110;

    chalResp->operations |= 0x80;

    memset((int*)((int)chalResp + 0x1D4), 0, 4);
    memset((int*)((int)chalResp + 0x1D8), 0, 4);
    memset((int*)((int)chalResp + 0x1DC), 0, 4);
    memset((int*)((int)chalResp + 0x1E0), 0, 4);
    memset((int*)((int)chalResp + 0x1E4), 0, 4);

    memset((long long*)((int)chalResp + 0x1E8), 0, 8);
    memset((long long*)((int)chalResp + 0x1F0), 0, 8);
    memset((long long*)((int)chalResp + 0x1F8), 0, 8);
    memset((long long*)((int)chalResp + 0x200), 0, 8);
    memset((long long*)((int)chalResp + 0x208), 0, 8);
    memset((long long*)((int)chalResp + 0x210), 0, 8);

    memset((int*)((int)chalResp + 0x218), 0, 4);
    memset((int*)((int)chalResp + 0x1D0), XboxHardwareInfo->Flags, 4);

    if ((XboxHardwareInfo->Flags & 0x20) == 0)
        return STATUS_SUCCESS;

    for (int i = 0; i < 5; i++) {
        memcpy(xamAllocatedData, (byte*)0x8E038400, 0x15C);
        XeCryptSha(xamAllocatedData, 0x5C, 0, 0, 0, 0, outDigest, XECRYPT_SHA_DIGEST_SIZE);
    
        if (!NT_SUCCESS(XeKeysGetKey(XEKEY_CONSTANT_SATA_DISK_SECURITY_KEY, &rsa, (PDWORD)&rsaSecuritySize)))
            continue;

        if (rsaSecuritySize != 0x110 || rsa.cqw != 0x20)
            continue;

        byte* signature = (byte*)(xamAllocatedData + 0x5C);

        XeCryptBnQw_SwapDwQwLeBe((PQWORD)signature, (PQWORD)signature, 0x20);
        if (XeCryptBnQwNeRsaPubCrypt((PQWORD)signature, (PQWORD)signature, &rsa))
            continue;

        XeCryptBnQw_SwapDwQwLeBe((PQWORD)signature, (PQWORD)signature, 0x20);
        if (XeCryptBnDwLePkcs1Verify(outDigest, signature, 0x100))
            break;
    }

    if (xamAllocatedData == 0)
        return STATUS_SUCCESS;

    memcpy((int*)(chalResp + 0x1D4), xamAllocatedData, 0x14);
    memcpy((int*)(*(int*)(chalResp + 0x1E8)), (int*)(xamAllocatedData + 0x14), 4);
    memcpy((int*)(*(int*)(chalResp + 0x1E8) + 0x4), (int*)(xamAllocatedData + 0x18), 4);

    return STATUS_SUCCESS;
}

NTSTATUS SysChall_SetupDiskVerificationHash(xoscResponse* chalResp) {
    BYTE pageData[0xA0] = { 0 };
    BYTE outDigest[0x14] = { 0 };

    for (int i = 0; i < 5; i++) {
        memcpy(pageData, (int*)0x8E038780, 0xA0);
        XeCryptSha((PBYTE)(pageData + 0x14), 0x8C, 0, 0, 0, 0, outDigest, XECRYPT_SHA_DIGEST_SIZE);

        if (memcmp((PBYTE)(pageData + 0x14), outDigest, 0x14) != 0)
            continue;

        goto nextSequence;
    }

    chalResp->sataResult = STATUS_KEY_RESULT_FAILED;
    return STATUS_SUCCESS;

nextSequence:
    chalResp->operations = 0;

    memcpy((byte*)((int)chalResp + 0x21C), (byte*)0x8E038680, 0x80);

    memcpy((int*)((int)chalResp + 0x24), (int*)(pageData + 0x14), 4);
    memcpy((int*)((int)chalResp + 0x28), (int*)(pageData + 0x18), 4);
    memcpy((int*)((int)chalResp + 0x2C), (int*)(pageData + 0x1C), 4);
    memcpy((int*)((int)chalResp + 0x30), (int*)(pageData + 0x90), 4);
    memcpy((long long*)((int)chalResp + 0xD0), (int*)(pageData + 0x70), 4);
    memcpy((int*)((int)chalResp + 0x2A4), (int*)(pageData + 0x9C), 4);
    memcpy((long long*)((int)chalResp + 0x2C8), (int*)(pageData + 0x34), 4);
    memcpy((long long*)((int)chalResp + 0x2D0), (int*)(pageData + 0x38), 4);

    memcpy((byte*)(pageData + 0x3C), (byte*)(pageData + 0x1F), 0x11);
    memcpy((byte*)((int)chalResp + 0x9D), (byte*)(pageData + 0x40), 0x20);
    memcpy((byte*)((int)chalResp + 0xBD), (byte*)(pageData + 0x60), 0x10);

    memcpy((long long*)((int)chalResp + 0xD8), (long long*)(pageData + 0x78), 8);
    memcpy((long long*)((int)chalResp + 0xE0), (long long*)(pageData + 0x80), 8);
    memcpy((long long*)((int)chalResp + 0xE8), (long long*)((pageData + 0x80) + 0x8), 8);
    memcpy((long long*)((int)chalResp + 0x29C), (long long*)(pageData + 0x94), 8);

    return chalResp->sataResult = STATUS_SUCCESS;
}

NTSTATUS SysChall_SetupSerialNumberHash(xoscResponse* chalResp) {
    byte outDigest[0x14] = { 0 };
    byte serialNumber[0xC] = { 0 };

    WORD keyProperty = XeKeysGetKeyProperties(XEKEY_CONSOLE_SERIAL_NUMBER);

    for (int i = 0; i < 5; i++) {
        memcpy(xamAllocatedData, (byte*)0x8E038000, 0x400);
        XeCryptSha((PBYTE)(xamAllocatedData + 0x14), 0x3EC, 0, 0, 0, 0, outDigest, XECRYPT_SHA_DIGEST_SIZE);
        
        if (memcmp(xamAllocatedData, outDigest, 0x14) != 0)
            continue;

        goto nextSequence;
    }

    chalResp->keyResultSerial = STATUS_KEY_RESULT_FAILED;
    return STATUS_SUCCESS;

nextSequence:
    if (xamAllocatedData == 0) {
        chalResp->keyResultSerial = STATUS_KEY_RESULT_FAILED;
        return STATUS_SUCCESS;
    }

    if (*(int*)(xamAllocatedData + 0x14) < 1) {
        chalResp->keyResultSerial = 0xC8003005;
        return STATUS_SUCCESS;
    }

    if (keyProperty < 0xC) {
        chalResp->keyResultSerial = STATUS_INVALID_PARAMETER_1;
        return STATUS_SUCCESS;
    }

    NTSTATUS keyStatus = XeKeysGetKey(XEKEY_CONSOLE_SERIAL_NUMBER, serialNumber, (PDWORD)&keyProperty);

    if (!NT_SUCCESS(keyStatus)) {
        chalResp->keyResultSerial = keyStatus;
        return STATUS_SUCCESS;
    }

    chalResp->operations |= 0x20;

    int size = (*(int*)(xamAllocatedData + 0x14) - 1);

    memcpy(&chalResp->consoleId, (long long*)(xamAllocatedData + 0x1A0), 8);
    memcpy((long long*)(*(int*)(xamAllocatedData + 0x1A0) + 0x8),
        (int*)(xamAllocatedData + 0x10),
        size = size <= 5 ? size : 5
    );

    if (size < 5 && (size - 5) != 0)
        memcpy((long long*)((int)xamAllocatedData + ((size + 0x35) * 8)), 0, (size - 5));

    memcpy((byte*)(xamAllocatedData + 0x138), serialNumber, 0xC);

    *(byte*)(xamAllocatedData + 0x144) = 0;
    return chalResp->keyResultSerial = STATUS_SUCCESS;
}

void SysChall_SetupModuleHashes(xoscResponse* chalResp) {
    chalResp->operations |= 0x8;

    DWORD flags = 0;
    SHORT settingSize = 6;
    byte unknownBuffer[0x10] = { 0 };
    byte securityDigest[0x14];
    byte macAddress[0x6];
    byte smcOut[0x5];
    byte smcMsg[0x5];

    memcpy(&chalResp->bootloaderVersion, (short*)0x8E038600, 2);
    memcpy(&chalResp->xamRegion, (short*)0x8E038602, 2);
    memcpy(&chalResp->xamOdd, (short*)0x8E038604, 2);

    memcpy(&chalResp->_unk3, (int*)0x8E03861C, 4);
    memcpy(&chalResp->flashSize, (int*)0x8E038618, 4);
    memcpy(&chalResp->xoscRegion, (int*)0x8E038614, 4);
    memcpy(&chalResp->hvFlags, (int*)0x8E038610, 4);

    memcpy(&chalResp->_unk10, (long long*)0x8E038630, 8);
    memcpy(&chalResp->_unk11, (long long*)0x8E038638, 8);
    memcpy(&chalResp->_unk12, (long long*)0x8E038640, 8);
    memcpy(&chalResp->hvProtectionFlags, (long long*)0x8E038678, 8);
    memcpy(&chalResp->_unk5, (long long*)0x8E038704, 8);

    memcpy(&chalResp->_unk6, (int*)0x8E038708, 4);
    memcpy(&chalResp->_unk8, (int*)0x8E03870C, 4);
    memcpy(&chalResp->_unk9, (int*)0x8E038710, 4);
    memcpy(&chalResp->crlVersion, (int*)0x8E000154, 4);

    PLDR_DATA_TABLE_ENTRY hXam = (PLDR_DATA_TABLE_ENTRY)GetModuleHandleA(MODULE_XAM);
    PLDR_DATA_TABLE_ENTRY hKernel = (PLDR_DATA_TABLE_ENTRY)GetModuleHandleA(MODULE_KERNEL);
    PLDR_DATA_TABLE_ENTRY hCurrTitle = (PLDR_DATA_TABLE_ENTRY)GetModuleHandleA(MODULE_TITLE);

    memcpy(securityDigest, (byte*)0x8E03AA40, 0x14);
    unknownBuffer[0xE] &= 0xF8;

    if (hXam || hKernel || hCurrTitle) {
        IMAGE_XEX_HEADER* xamHeader = (IMAGE_XEX_HEADER*)(hXam->XexHeaderBase);

        if (xamHeader) {
            XEX_SECURITY_INFO* securityInfo = (XEX_SECURITY_INFO*)(xamHeader->SecurityInfo);

            int size = ((xamHeader->SizeOfHeaders - ((int)xamHeader->SecurityInfo + 0x17C)) + (int)xamHeader);
            XeCryptSha((PBYTE)&securityInfo->AllowedMediaTypes, size, securityDigest, 0x14, unknownBuffer, 0x10, securityDigest, 0x14);
        }

        IMAGE_XEX_HEADER* krnlHeader = (IMAGE_XEX_HEADER*)(hKernel->XexHeaderBase);
        if (krnlHeader) {
            if (NT_SUCCESS(ExGetXConfigSetting(XCONFIG_SECURED_CATEGORY, XCONFIG_SECURED_MAC_ADDRESS, macAddress, 0x6, (PWORD)settingSize))) {
                XEX_SECURITY_INFO* securityInfo = (XEX_SECURITY_INFO*)(krnlHeader->SecurityInfo);

                int size = ((krnlHeader->SizeOfHeaders - ((int)krnlHeader->SecurityInfo + 0x17C)) + (int)krnlHeader);
                XeCryptSha((PBYTE)&securityInfo->AllowedMediaTypes, size, securityDigest, 0x14, macAddress, 0x6, securityDigest, 0x14);
            }
        }

        IMAGE_XEX_HEADER* currModuleHeader = (IMAGE_XEX_HEADER*)(hCurrTitle->XexHeaderBase);
        if (currModuleHeader) {
            smcMsg[0] = REQUEST_SMC_VERSION;
            HalSendSMCMessage(smcMsg, smcOut);

            XEX_SECURITY_INFO* securityInfo = (XEX_SECURITY_INFO*)(currModuleHeader->SecurityInfo);

            int size = ((currModuleHeader->SizeOfHeaders - (DWORD)&securityInfo->AllowedMediaTypes) + (int)currModuleHeader);
            XeCryptSha((PBYTE)&securityInfo->AllowedMediaTypes, size, securityDigest, 0x14, smcOut, 0x5, securityDigest, 0x14);
        }
    }

    XeCryptSha((PBYTE)0x900101A3, 0x8E59, securityDigest, 0x14, 0, 0, securityDigest, 0x14);
    securityDigest[0] = 7;

    memcpy(chalResp->kvDigest, securityDigest, 0x10);
    memcpy(chalResp->fuseDigest, (byte*)0x8E03AA50, 0x10);
}

NTSTATUS SysChall_GetPCIEDriveConnectionStatus(xoscResponse* chalResp) {
    byte data[0x100] = { 0 };
    HalReadWritePCISpace(0, 2, 0, 0, data, 0x100, 0);

    QWORD r9 = (((*(byte*)(data + 0x8) & ~0xFFFF00) | ((*(short*)(data + 0x2) << 8) & 0xFFFF00) << 8) & 0xFFFFFFFFFFFFFFFF);
    QWORD r10 = (((*(byte*)(data + 0xB) & ~0xFFFF00) | ((*(short*)(data + 0x4) << 8) & 0xFFFF00) << 8) & 0xFFFFFFFFFFFFFFFF);

    chalResp->daeResult = 0x40000012;
    chalResp->operations |= 0x100;
    chalResp->pcieHardwareInfo = ((((r9 | XboxHardwareInfo->PCIBridgeRevisionID) << 32) | r10) | *(byte*)(data + 0xA));

    return STATUS_SUCCESS;
}

NTSTATUS SysChall_Init(int task, char* tableName, int tableSize, xoscResponse* chalResp, int bufferSize) {
    if (chalResp == 0 || bufferSize == 0 || bufferSize < 0x2E0) {
        printf("[SysChall_Execute] failed\n");
        return E_INVALIDARG;
    }

    NTSTATUS exeIdStatus = STATUS_SUCCESS;
    PXEX_EXECUTION_ID executionId;

    memset(chalResp, 0, bufferSize);
    memset(chalResp, 0xAA, 0x2E0);

    chalResp->operations = 0;
    chalResp->xoscMajor = 9;
    chalResp->xoscFooterMagic = 0x5F534750;

    if (NT_SUCCESS(XamAlloc(0x200000, 0x8000, xamAllocatedData))) {
        SysChall_GetDeviceControlRequest(chalResp);
        SysChall_GetConsoleKVCertificate(chalResp);
        SysChall_SetupModuleHashes(chalResp);
        SysChall_GetStorageDeviceSizes(chalResp);
        SysChall_GetPCIEDriveConnectionStatus(chalResp);
    }

    if (NT_SUCCESS(exeIdStatus = XamGetExecutionId(&executionId))) {
        chalResp->executionResult = exeIdStatus;

        memcpy(&chalResp->xexExecutionId, &executionId, sizeof(XEX_EXECUTION_ID));
        XamLoaderGetMediaInfo(&chalResp->mediaInfo, &chalResp->titleId);

        chalResp->operations |= 0x4;
    }

    if (XamLoaderIsTitleTerminatePending())
        chalResp->operations |= XOSC_FLAG_STATUS_FLAG_TITLE_TERMINATED;

    if (XamTaskShouldExit())
        chalResp->operations |= XOSC_FLAG_STATUS_TASK_SHOULD_EXIT;

    if (xamAllocatedData != 0)
        XamFree(xamAllocatedData);

    return chalResp->result = STATUS_SUCCESS;
}