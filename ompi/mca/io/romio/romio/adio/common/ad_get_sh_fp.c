/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* 
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "adio.h"

#ifdef ROMIO_BGL
void ADIOI_BGL_Get_shared_fp(ADIO_File fd, int size, ADIO_Offset *shared_fp, int *error_code);
#endif
#ifdef ROMIO_BG
void ADIOI_BG_Get_shared_fp(ADIO_File fd, int size, ADIO_Offset *shared_fp, int *error_code);
#endif

/* returns the current location of the shared_fp in terms of the
   no. of etypes relative to the current view, and also increments the
   shared_fp by the number of etypes to be accessed (incr) in the read
   or write following this function. */

void ADIOI_NFS_Get_shared_fp(ADIO_File fd, int incr, ADIO_Offset *shared_fp, 
                             int *error_code);

void ADIO_Get_shared_fp(ADIO_File fd, int incr, ADIO_Offset *shared_fp, 
			 int *error_code)
{
    ADIO_Status status;
    ADIO_Offset new_fp;
    MPI_Comm dupcommself;

#ifdef ROMIO_NFS
    if (fd->file_system == ADIO_NFS) {
	ADIOI_NFS_Get_shared_fp(fd, incr, shared_fp, error_code);
	return;
    }
#endif

#ifdef ROMIO_BGL
    /* BGLOCKLESS won't support shared fp */
    if (fd->file_system == ADIO_BGL) {
	ADIOI_BGL_Get_shared_fp(fd, incr, shared_fp, error_code);
	return;
    }
#endif

    if (fd->shared_fp_fd == ADIO_FILE_NULL) {
	MPI_Comm_dup(MPI_COMM_SELF, &dupcommself);
	fd->shared_fp_fd = ADIO_Open(MPI_COMM_SELF, dupcommself, 
				     fd->shared_fp_fname, 
				     fd->file_system,
				     fd->fns,
				     ADIO_CREATE | ADIO_RDWR | ADIO_DELETE_ON_CLOSE, 
				     0, MPI_BYTE, MPI_BYTE,
				     MPI_INFO_NULL, 
				     ADIO_PERM_NULL, error_code);
	if (*error_code != MPI_SUCCESS) return;
	*shared_fp = 0;
	ADIOI_WRITE_LOCK(fd->shared_fp_fd, 0, SEEK_SET, sizeof(ADIO_Offset));
	ADIO_ReadContig(fd->shared_fp_fd, shared_fp, sizeof(ADIO_Offset), 
		       MPI_BYTE, ADIO_EXPLICIT_OFFSET, 0, &status, error_code);
        /* if the file is empty, the above function may return error
           (reading beyond end of file). In that case, shared_fp = 0, 
           set above, is the correct value. */
    }
    else {
	ADIOI_WRITE_LOCK(fd->shared_fp_fd, 0, SEEK_SET, sizeof(ADIO_Offset));
	ADIO_ReadContig(fd->shared_fp_fd, shared_fp, sizeof(ADIO_Offset), 
		       MPI_BYTE, ADIO_EXPLICIT_OFFSET, 0, &status, error_code);
	if (*error_code != MPI_SUCCESS) {
	    ADIOI_UNLOCK(fd->shared_fp_fd, 0, SEEK_SET, sizeof(ADIO_Offset));
	    return;
	}
    }

    if (incr == 0) {goto done;}

    new_fp = *shared_fp + incr;

    ADIO_WriteContig(fd->shared_fp_fd, &new_fp, sizeof(ADIO_Offset), 
		    MPI_BYTE, ADIO_EXPLICIT_OFFSET, 0, &status, error_code);
done:
    ADIOI_UNLOCK(fd->shared_fp_fd, 0, SEEK_SET, sizeof(ADIO_Offset));
}
