//
// OsrFilterForwardCreateIrp
//
//  This routine is an allows a Filter Driver user to send a Create Irp to a Device
//  object to a device lower than the this Filter Filter. I.E allows the Filter to
//  do a ZwCreateFile to open up a file, however the create does not get 
//  forwarded to the top of the storage stack, but instead gets sent directly to
//  the Device Object below this Drivers device Object.
//
//
//      Note:   In order for this routine to work, the filter driver utilizing this 
//              code must create an extra device object with a unique device name so that
//              This routine can forward the IRPs to that device object which will in turn
//              forward them to the driver we are filtering when when ZwCreateFile is called. 
//
//      Note:   Be aware that you can not use any Zw routines (except for ZwClose) utilizing 
//              the handle returned from this routine.  Because if you do, the call will go 
//              back to the top of the device stack. 
//              Therefore you must call ObReferenceObjectByHandle to get the associated File 
//              Object and then build your own IRPs for subsequent requests that you want to 
//              make regarding this file. 
//
// Inputs:
//  DesiredAccess - type of access the user requies to the file or directory.
//  ObjectAttributes - Poiner to initialized object attributes
//  AllocationSize - optional size of default allocation.
//  FileAttributes - file attributes
//  ShareAccess - type of share access
//  Disposition - type of action to be taken depending on whether the file already exists
//  CreateOptions - options to be applied when opening or creating the file.
//  EaBuffer - optional.
//  EaLength - optional
//  CreateFileType - must be CreateFileTypeNone.
//  ExtraCreateParameters - must be null.
//  Options - options to be used during creation of create request
//  DeviceObject - Pointer to the device object to which the create request is to be sent. 
//    The device object must be a filter or file system device object in the file system 
//    driver stack for the volume on which the file or directory resides. This parameter 
//    is optional and can be NULL. If this parameter is NULL, the request will be sent 
//    to the device object at the top of the driver stack. 
//
// Outputs:
//  FileHandle - address to receive file handle if successful.
//  IoStatusBlock - pointer of structure to receive final I/O status.
//
// Returns:
//  returns whatever IoCreateFileSpecifyDeviceObjectHint returns.
//
// Notes:
//  This is only supported in W2K or later.
//
extern "C"
NTSTATUS OSR_FILTER_API OsrFilterForwardCreateIrp(OUT PHANDLE  FileHandle,
    IN ACCESS_MASK  DesiredAccess,IN POBJECT_ATTRIBUTES  ObjectAttributes,
    OUT PIO_STATUS_BLOCK  IoStatusBlock,IN PLARGE_INTEGER  AllocationSize OPTIONAL,
    IN ULONG  FileAttributes,IN ULONG  ShareAccess,IN ULONG  Disposition,IN ULONG  CreateOptions,
    IN PVOID  EaBuffer OPTIONAL,IN ULONG  EaLength,
    IN CREATE_FILE_TYPE  CreateFileType,IN PVOID  ExtraCreateParameters OPTIONAL,
    IN ULONG  Options,IN PVOID  DeviceObject)
{
    NTSTATUS    status;
#ifdef NT_XP

    status = IoCreateFileSpecifyDeviceObjectHint(FileHandle,
                                DesiredAccess,ObjectAttributes,
                                IoStatusBlock,AllocationSize,
                                FileAttributes,ShareAccess,Disposition,CreateOptions,
                                EaBuffer,EaLength,
                                CreateFileType,ExtraCreateParameters,
                                Options,DeviceObject);

#else  // NT_XP
    UNICODE_STRING          QQString = {sizeof(L"??")-sizeof(WCHAR),sizeof(L"??"),L"??"};
    UNICODE_STRING          DosDevString = {sizeof(L"DosDevices")-sizeof(WCHAR),sizeof(L"DosDevices"),L"DosDevices"};
    UNICODE_STRING          GlobalString = {sizeof(L"GLOBAL??")-sizeof(WCHAR),sizeof(L"GLOBAL??"),L"GLOBAL??"};

    //
    // Well, the bad news is that this is not WXP or later so we have to implement the
    // functionality of IoCreateFileSpecifyDeviceObjectHint ourselves.   This entails
    // parsing the input name into a name we can handle and then passing it to the
    // shadow device object that we created earlier which will forward the Irp to
    // a FS or Filter below us..
    //
    if(DeviceObject) { 

        UNICODE_STRING      newFileName;
        UNICODE_STRING      parsedFileName = {0,0,NULL};
        OBJECT_ATTRIBUTES   newObjectAttributes;
        ULONG               sizeNeeded = ObjectAttributes->ObjectName->Length + 
                                (sizeof(DeviceObject) * (2 * sizeof(WCHAR)));
        UNICODE_STRING      objectName;

        objectName = *ObjectAttributes->ObjectName;

        //
        // Parse the input name.
        //
        status = ParseInputName((PDEVICE_OBJECT) DeviceObject,&objectName);

        if(!NT_SUCCESS(status)) {
            //
            // An error occurred, tell the caller.
            //
            return status;
        }

        //
        // Determine how much memory we need for the name to be built.
        //
        sizeNeeded += sizeof(L"\\Device\\\\") + sizeof(WCHAR);  // \Device\xxxxxxxx\.......

        newFileName.Buffer = (PWSTR) ExAllocatePool(PagedPool,sizeNeeded);

        if(!newFileName.Buffer) {
            //
            // No memory allocated, return the error.
            //
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Initialize the buffer to hold the name.
        //
        RtlZeroMemory(newFileName.Buffer,sizeNeeded);

        //
        // Build the appropriate name for the create, so that it goes to our
        // shadow device object....
        //

#if !defined(_WIN64)
        swprintf(newFileName.Buffer,L"\\Device\\%08.8X",(ULONG_PTR) DeviceObject);
#else  // !defined(_WIN64)
        swprintf(newFileName.Buffer,L"\\Device\\%016.16X",(ULONG_PTR) DeviceObject);
#endif // !defined(_WIN64)

        wcsncat(newFileName.Buffer,objectName.Buffer,(objectName.Length)/sizeof(WCHAR));
        newFileName.Length = (USHORT) wcslen(newFileName.Buffer)*sizeof(WCHAR);
        newFileName.MaximumLength = (USHORT) sizeNeeded;
        memcpy(&newObjectAttributes,ObjectAttributes,sizeof(OBJECT_ATTRIBUTES));
        newObjectAttributes.ObjectName = &newFileName;

#if DBG_NAME
        DbgPrint("OsrFilterForwardCreateIrp Create: File Name %wZ\n",&newFileName);
#endif 

        //
        // Perform the Create to the Shadow Object.
        //
        status = ZwCreateFile(FileHandle,DesiredAccess,&newObjectAttributes,IoStatusBlock,
                            AllocationSize,FileAttributes,ShareAccess,Disposition,
                            CreateOptions,EaBuffer,EaLength);

        //
        // Clean up the allocated memory...
        //
        ExFreePool(newFileName.Buffer);
        ExFreePool(objectName.Buffer);

    } else {

        //
        // No Hint specified, just do a normal create which will go to the top
        // of the stack....
        //
        status = ZwCreateFile(FileHandle,DesiredAccess,ObjectAttributes,IoStatusBlock,
                            AllocationSize,FileAttributes,ShareAccess,Disposition,
                            CreateOptions,EaBuffer,EaLength);

    }

#endif // NT_XP
    return status;

}



//
// ParsePath
//
//  This routine is an internal routine called to parse the input
//  file path into its constituent parts...
//
// Inputs:
//  PComponentHead - address of a ListHead which will receive the linked
//                   list of PATH_ENTRY structures containing the parts
//                   of the input name.
//  PPath - Path Name to parse.
//
// Outputs:
//  None.
//
// Returns:
//  STATUS_SUCCESS if the name is parsed successfully, an error otherwise.
//
// Notes:
//  This is only supported in W2K or later.
//

NTSTATUS ParsePath(PLIST_ENTRY PComponentHead, PUNICODE_STRING PPath)
{
	PATH_ENTRY      *pe;
    NTSTATUS        status = STATUS_INSUFFICIENT_RESOURCES;
    UNICODE_STRING  nextComponent;
    PUNICODE_STRING pNextComponent;
    UNICODE_STRING  remainingName;
    BOOLEAN         bFirstPass = TRUE;

    remainingName = *PPath;

    //
    // Loop through a name parsing it into its path pieces.  We do this
    // using FsRtlDissectName.
    //
    while (remainingName.Length > 0) {

        pNextComponent = &nextComponent;

        FsRtlDissectName(remainingName, &nextComponent, &remainingName);

        //
        // Allocate a PATH_ENTRY structure to hold the dissected part of
        // the name.
        //
	    pe = (PPATH_ENTRY) ExAllocatePoolWithTag(PagedPool,sizeof(PATH_ENTRY),'2RSO');

        ULONG size = nextComponent.Length+sizeof(WCHAR);

        //
        // See if the memory was allocated.  If not, cleanup after ourselves.
        //
        if(!pe) {
            EmptyPathList(PComponentHead);
            return status;
        }

        //
        // Got the memory, initialize it.
        //
        RtlZeroMemory(pe,sizeof(PATH_ENTRY));

        //
        // see if this is the first pass through the list.  If it is, see if
        // the name is preceeded by \?? or by \DosDevices.  If it is,
        // we substitue \global??
        //

        if(bFirstPass) {

#ifdef NT_XP
            if(RtlCompareUnicodeString(&QQString,&nextComponent,FALSE) == 0) {

                pNextComponent = &GlobalString;
#else // NT_XP
            if(RtlCompareUnicodeString(&GlobalString,&nextComponent,FALSE) == 0) {

                pNextComponent = &QQString;
#endif // NT_XP

            } else if(RtlCompareUnicodeString(&DosDevString,&nextComponent,FALSE) == 0) {

#ifdef NT_XP
                pNextComponent = &GlobalString;
#else // NT_XP
                pNextComponent = &QQString;
#endif // NT_XP

            }

            bFirstPass = FALSE;
        }

        //
        // Allocate memory for the path.
        //

        pe->Path = (PWCHAR) ExAllocatePoolWithTag(PagedPool,pNextComponent->MaximumLength+sizeof(WCHAR),'3RSO');
        if(!pe->Path) {

            //
            // We failed to allocate memory, cleanup and exit.
            //
            ExFreePool(pe);
            EmptyPathList(PComponentHead);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Add the part of the parsed name and then continue parsing.
        //

        RtlZeroMemory(pe->Path,pNextComponent->MaximumLength+sizeof(WCHAR));
        wcsncpy(pe->Path,pNextComponent->Buffer,pNextComponent->Length/sizeof(WCHAR));

        InsertTailList(PComponentHead,&pe->ListEntry);

    }

    return STATUS_SUCCESS;
}



//
// EmptyPathList
//
//  This routine is an internal routine called to delete all the 
//  entries in a path list.
//
// Inputs:
//  PListHead - address of a ListHead which will receive the linked
//                   list of PATH_ENTRY structures containing the parts
//                   of the input name.
//
// Outputs:
//  None.
//
// Returns:
//  None.
//
// Notes:
//  None.
//
void EmptyPathList(PLIST_ENTRY PListHead)
{
    while(!IsListEmpty(PListHead)) {
        PPATH_ENTRY pe = (PPATH_ENTRY) RemoveHeadList(PListHead);
        ExFreePool(pe->Path);
        ExFreePool(pe);
    }
}

#ifndef NT_XP



//
// AnalyzePath
//
//  This routine is an internal routine called to analyze the input
//  file path in order to determine if there is a DeviceObject that will 
//  handle the input name.
//
// Inputs:
//  PHintDeviceObject - address of the device object that must lie in the
//                    path of the device object which handles this name.  
//  PComponentHead - address of a ListHead which contains the linked
//                   list of PATH_ENTRY structures containing the parts
//                   of the input name.
//
// Outputs:
//  PFinalName - address to receive the PFinalName i.e. the part of the
//        name to be passed to the FS for handling.
//
// Returns:
//  STATUS_SUCCESS if the name is parsed successfully, an error otherwise.
//
// Notes:
//  This is only supported in W2K or later.
//

NTSTATUS AnalyzePath(PDEVICE_OBJECT PHintDeviceObject,PLIST_ENTRY PComponentListHead,PWCHAR* PFinalName)
{
    PPATH_ENTRY     ppe = (PPATH_ENTRY) PComponentListHead->Flink;
    PDEVICE_OBJECT  pDeviceObject;
    PFILE_OBJECT    pFileObject;
    ULONG           size = 0;
    ULONG           components = 0;
    PWCHAR          pBuffer = NULL;
    UNICODE_STRING  path = { 0, 0, NULL};
    NTSTATUS        status = STATUS_INSUFFICIENT_RESOURCES;

    *PFinalName = NULL;

    //
    // Figure out the full size of the name that can be built out of the parsed name
    // parts.
    //
    while(TRUE && ((PLIST_ENTRY) ppe != PComponentListHead)) {

        size += wcslen(ppe->Path)*sizeof(WCHAR);
        components++;
        ppe = (PPATH_ENTRY) ppe->ListEntry.Flink;

    }

    //
    // No Size, no problem, just exit with an error.
    //
    if(!size) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    size += ((1 + components) * sizeof(WCHAR));

    //
    // Allocate a name for the string,  if we fail, we
    // exit.
    //
    pBuffer = (PWCHAR) ExAllocatePoolWithTag(PagedPool,size,'4RSO');

    if(!pBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the memory and prepare to rebuild the
    // name piece by piece looking for a device that corresponds
    // to the built name.
    //
    RtlZeroMemory(pBuffer,size);

    ppe = (PPATH_ENTRY) PComponentListHead->Flink;

    components = 0;

    while((PLIST_ENTRY) ppe != PComponentListHead) {

        components++;

        //
        // Prefix the name piece being built with a '\'.
        //
        wcscat(pBuffer,L"\\");
        wcscat(pBuffer,ppe->Path);

        //
        // Set up the unicode string which represents the
        // name we will query with.
        //
        path.Length = wcslen(pBuffer)*sizeof(WCHAR);
        path.MaximumLength = path.Length;
        path.Buffer = pBuffer;

        //
        // See if we find a device object which corresponds to the name.
        //
        status = IoGetDeviceObjectPointer(&path,FILE_ANY_ACCESS,&pFileObject,&pDeviceObject);

        //
        // See if we found a match.
        //
        if(NT_SUCCESS(status)) {

            BOOLEAN bFound = FALSE;
            PDEVICE_OBJECT pFsObject = NULL;
            PDEVICE_OBJECT pNextDeviceObject;

            //
            // We have a match, see if we can find the Base File System Device
            // object from the input file object.
            //
            pFsObject = IoGetBaseFileSystemDeviceObject(pFileObject);

            if(!pFsObject) {

                //
                // Well this did not work, so exit with an error.
                //
                ObDereferenceObject(pFileObject);
                ExFreePool(pBuffer);
                return STATUS_OBJECT_NAME_NOT_FOUND;

            }

            //
            // Get the FS Device Object from the VPB in order to start out start our
            // search.
            //
            if(pFsObject->Vpb) {
                pNextDeviceObject = pFsObject->Vpb->DeviceObject;
            } else {
                //
                // There is no VPB.  Let's assume this is a Network device 
                // we have been handed.  
                pNextDeviceObject = pFsObject;
            }

            //
            // Okay, I've found a device object.   Let's see if we can find the hint.
            //
            while(pNextDeviceObject) {

                if(pNextDeviceObject == PHintDeviceObject) {
                    bFound = TRUE;
                    break;
                }
                pNextDeviceObject = pNextDeviceObject->AttachedDevice;
            }

            ObDereferenceObject(pFileObject);

            if(!bFound) {

                //
                // Didn't find the hint, so abort the create....
                //
                ExFreePool(pBuffer);
                return STATUS_OBJECT_NAME_NOT_FOUND;

            }

            //
            // We found a handler for the input name and we found
            // the hint device object.   Build the left over parts
            // of the name into the Final Name and return that to 
            // to the caller so that the caller can get it passed
            // to the handler for this create call.
            //
            RtlZeroMemory(pBuffer,size);

            wcscat(pBuffer,L"\\");

            ppe = (PPATH_ENTRY) ppe->ListEntry.Flink;

            while(TRUE && ((PLIST_ENTRY) ppe != PComponentListHead)) {
                wcscat(pBuffer,ppe->Path);
                ppe = (PPATH_ENTRY) ppe->ListEntry.Flink;
                if((PLIST_ENTRY) ppe != PComponentListHead) {
                    wcscat(pBuffer,L"\\");
                } else {
                    break;
                }

            }

            //
            // Return the handled name and tell the caller how happy we
            // were to process his function call.
            //
            *PFinalName = pBuffer;
        
            return status;

        }

        //
        // No handler for the name found, continue appending parts of the name.
        //
        ppe = (PPATH_ENTRY) ppe->ListEntry.Flink;

    }

    ExFreePool(pBuffer);

    return status;
}



//
// ParseInputName
//
//  This routine is an internal routine called to Parse the input name.
//
// Inputs:
//  PHintDeviceObject - address of the device object that must lie in the
//                    path of the device object which handles this name.  
//  PPathName - path name to parse.
//
// Outputs:
//  None.
//
// Returns:
//  STATUS_SUCCESS if the name is parsed successfully, an error otherwise.
//
// Notes:
//  This is only supported in W2K or later.
//

NTSTATUS ParseInputName(PDEVICE_OBJECT HintDeviceObject,PUNICODE_STRING PPathName)
{
    PWCHAR      pBuffer = NULL;
    ULONG       tokenCount = 0;
    LIST_ENTRY  ComponentListHead;
    NTSTATUS    status;
    PWCHAR      pFinalName = NULL;

    if(!PPathName) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
        
    if(!PPathName->Buffer) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    //
    // Initialize the ListHead which will contain the parsed name parts.
    //
    InitializeListHead(&ComponentListHead);

    //
    // Parse the input name into the name parts.
    //
    status = ParsePath(&ComponentListHead,PPathName);

    if(!NT_SUCCESS(status)) {

        ExFreePool(pBuffer);
        return status;

    }

    //
    // Analyze the components of the name....
    //

    status = AnalyzePath(HintDeviceObject,&ComponentListHead,&pFinalName);

    if(NT_SUCCESS(status)) {

        //
        // Set up the new path name we are going to use....
        //

        PPathName->Buffer = pFinalName;
        PPathName->Length = wcslen(pFinalName)*sizeof(WCHAR);
        PPathName->MaximumLength = PPathName->Length;

    }

    //
    // Clean up the components of the name....
    //
    EmptyPathList(&ComponentListHead);

    //
    // Tell them how happy we are.
    //

    return status;
}

NTSTATUS GetFullPathName(PFILE_OBJECT PFileObject,PUNICODE_STRING PNameString)
{
    PFILE_OBJECT relatedFileObject;
    ULONG        pathLen;
    PWCHAR       pathOffset;
    NTSTATUS status = STATUS_OBJECT_NAME_NOT_FOUND;

    PNameString->Length = 0;
    PNameString->MaximumLength = 0;
    PNameString->Buffer = 0;

    //
    // Do this in an exception handling block, in case of mangled names in the
    // file object
    //

    __try {

        //
        // Now, create the full path name. First, calculate the length taking into 
        // account space for seperators and the leading drive letter plus ':'
        //

        pathLen = PFileObject->FileName.Length;

        relatedFileObject = PFileObject->RelatedFileObject;
    
        //
        // Only look at related file object if this is a relative name
        //

        if( PFileObject->FileName.Length && PFileObject->FileName.Buffer[0] != L'\\' )  {

            while( relatedFileObject ) {

                pathLen += relatedFileObject->FileName.Length;

                if((relatedFileObject->FileName.Length) && 
                   (relatedFileObject->FileName.Buffer[(relatedFileObject->FileName.Length/sizeof(WCHAR))-1] != L'\\')) {
                    pathLen += sizeof(WCHAR);
                }

                relatedFileObject = relatedFileObject->RelatedFileObject;
            }
    
        }

        PWCHAR pBuffer = (PWCHAR) ExAllocatePoolWithTag(PagedPool,pathLen,'nRSO');

        if(!pBuffer) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(pBuffer,pathLen);
        PNameString->Buffer = pBuffer;
        PNameString->MaximumLength = (USHORT) pathLen;
        PNameString->Length = (USHORT) pathLen;

        //
        // If there is no deviceInfo, it means that it is a network drive.
        //

        //sprintf( fullPathName, L"\\." );
    
        //
        // Now, start at the end and work back to the beginning of the path
        //

        pathOffset = (PWCHAR) ((PUCHAR) pBuffer + pathLen - PFileObject->FileName.Length);

        wcsncpy( pathOffset, PFileObject->FileName.Buffer, PFileObject->FileName.Length/sizeof(WCHAR) );

        relatedFileObject = PFileObject->RelatedFileObject;
    
        if( PFileObject->FileName.Buffer[0] != L'\\' )  {

            while( relatedFileObject ) {

                if((relatedFileObject->FileName.Length) && 
                   (relatedFileObject->FileName.Buffer[(relatedFileObject->FileName.Length/sizeof(WCHAR))-1] != L'\\')) {
                    *(pathOffset - 1) = L'\\';
                    pathOffset = (PWCHAR) ((PUCHAR) pathOffset - (relatedFileObject->FileName.Length + sizeof(WCHAR)));
                } else {
                    pathOffset = (PWCHAR) ((PUCHAR) pathOffset - (relatedFileObject->FileName.Length));
                }

                wcsncpy( pathOffset, relatedFileObject->FileName.Buffer,
                        relatedFileObject->FileName.Length/sizeof(WCHAR) );

                relatedFileObject = relatedFileObject->RelatedFileObject;
            }
        }  

        //
        // If we added two '\' to the front because there was a relative file object
        // that specified the root directory, remove one
        //

//        if( pathLen > 3 && fullPathName[2] == '\\' && fullPathName[3] == '\\' )  {
        
//            strcpy( fullPathName + 2, fullPathName + 3 );
//        }

        status = STATUS_SUCCESS;

    } __except( EXCEPTION_EXECUTE_HANDLER ) {

    }

    return status;
}


//
// SendCreateIrpBelowOurFilter
//
//  This routine is an internal routine called to Send the input IRP
//  to the specified device object.
//
// Inputs:
//  DeviceObject - address of the device object that will receive the input
//                 irp.
//  Irp - irp to send
//
// Outputs:
//  None.
//
// Returns:
//  returns whatever IoCallDriver returns.
//
// Notes:
//  This is only supported in XP or later.
//

NTSTATUS SendCreateIrpBelowOurFilter(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION      currentStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION      nextStackLocation = IoGetNextIrpStackLocation(Irp);
    POSR_FILTER_EXT    shadowExt = (POSR_FILTER_EXT) DeviceObject->DeviceExtension;
    PFILE_OBJECT            pFileObject = currentStackLocation->FileObject;

    (void) GetExtension(DeviceObject, &shadowExt);

    OsrAssert(shadowExt);

    OsrAssert(shadowExt->ShadowedDeviceObject);

    //
    // Set up the Irp to be sent down to the filtered Device Object....
    //

    IoCopyCurrentIrpStackLocationToNext(Irp);

    nextStackLocation->DeviceObject = shadowExt->FilteredDeviceObject;

    //
    // Send it below.....
    //

    status = IoCallDriver(shadowExt->FilteredDeviceObject,Irp);

    //
    // Release the extension
    //

    DereferenceExtension(shadowExt, __LINE__);

    return status;
}
#endif // NT_XP
