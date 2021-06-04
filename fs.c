// ============================================================================
// fs.c - user FileSytem API
// ============================================================================

#include "bfs.h"
#include "fs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) { 
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0; 
}



// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and 
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE* fp = fopen(BFSDISK, "w+b");
  if (fp == NULL) FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp);               // initialize Super block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitInodes(fp);                  // initialize Inodes block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitDir(fp);                     // initialize Dir block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitFreeList();                  // initialize Freelist
  if (ret != 0) { fclose(fp); FATAL(ret); }

  fclose(fp);
  return 0;
}


// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE* fp = fopen(BFSDISK, "rb");
  if (fp == NULL) FATAL(ENODISK);           // BFSDISK not found
  fclose(fp);
  return 0;
}



// ============================================================================
// Open the existing file called 'fname'.  On success, return its file 
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname);        // lookup 'fname' in Directory
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort
// ============================================================================
i32 fsRead(i32 fd, i32 numb, void* buf) {
    i32 inum = bfsFdToInum(fd); //inum of the file
    i32 cursor = bfsTell(fd);  // cursor
    i32 fbn = cursor / BYTESPERBLOCK;  // fbn to start reading from
    i32 dbn = bfsFbnToDbn(inum, fbn); // dbn to get data from
    i8 bioBuf[BYTESPERBLOCK]; // to read data in that DBN
    char * tempBuf=(char* )buf;  // cast the data from buf to tempBuf
    int start = cursor - fbn * BYTESPERBLOCK;  // where to start wrting from
    int count = 0;// keep tracks of reached the numb

    int end = BYTESPERBLOCK; //the read buf end = 512

    int EndOfFile=fsSize(fd); //where the file ends 

    if(numb>(EndOfFile-cursor)) // if no enough bytes to be read
        numb=EndOfFile-cursor;  
    int j=0;
    while(1)
    {
       bfsRead(inum,fbn,bioBuf); // fetch data from the dbn
       int i=start; 
       for(;i<end;i++) //till the end of bioBuf. break if count reaches numb
       {
        if(count==numb)break;
        tempBuf[j]=bioBuf[i];
        j++;
        count++;
        }
      if(count<numb) // if there are still number of bytes to be read
      {
        start=0;
        fbn=fbn+1; //swtich to the next fbn
        dbn=bfsFbnToDbn(inum,fbn); // get the dbn 
        if(dbn==ENODBN)break; // if dbn does not exit break
       }
      else { // if numb of bytes is read, move the cursor to the numb of read
        bfsSetCursor(inum,count+cursor);
        break; // stop reading 
        }
    }
    return count;    // number of bytes read
}


// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0) FATAL(EBADCURS);
 
  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);
  
  switch(whence) {
    case SEEK_SET:
      g_oft[ofte].curs = offset;
      break;
    case SEEK_CUR:
      g_oft[ofte].curs += offset;
      break;
    case SEEK_END: {
        i32 end = fsSize(fd);
        g_oft[ofte].curs = end + offset;
        break;
      }
    default:
        FATAL(EBADWHENCE);
  }
  return 0;
}



// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) {
  return bfsTell(fd);
}



// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}



// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void* buf) {

    i32 inum = bfsFdToInum(fd); //inum of the file
    i32 cursor = bfsTell(fd);  // cursor
    i32 fbn = cursor / BYTESPERBLOCK;  // fbn to start writing from
    i32 dbn = bfsFbnToDbn(inum, fbn); // dbn to get data from
    i8 bioBuf[BYTESPERBLOCK]; // to read data in that DBN
    char * tempBuf=(char* )buf;  // cast the data from buf to tempBuf
    int start = cursor - fbn * BYTESPERBLOCK;  // where to start wrting from


    int count = 0;// keep tracks of reached the numb

    int end = BYTESPERBLOCK; //the read buf end = 512
    int j=0;  
    while(1){ // till numb of bytes are writen to bioBuf 
      bfsRead(inum,fbn,bioBuf); // fetch data from dbn
      int i=start;
      for(;i<end;i++) // writes all the way to the end of bioBuf. Breaks if count reaches numb
      {
        if(count==numb)break;
        bioBuf[i]=tempBuf[j];
        j++;
        count++;
      }

     if(count!=numb){ // if numb of bytes are not written 
       bioWrite(dbn,bioBuf); // update the current fbn
       start=0; // begining of new fbn bioBuf
       fbn=fbn+1; // switch to the next fbn
       dbn=bfsFbnToDbn(inum,fbn);
       if(dbn==ENODBN){ // if that fbn does  not exist
        dbn=bfsAllocBlock(inum,fbn); // allocate a block
        bfsSetSize(inum,fsSize(fd)+(numb-count)); //increase the size to the numb of bytes remaining 
        }
       }

      else{ // if numb of bytes is writen 
        bioWrite(dbn,bioBuf); //update the dbn
        bfsSetCursor(inum,count+cursor); // move the cursor to the new spot
        break; //stop write
      }
    }
    return 0; 
}
