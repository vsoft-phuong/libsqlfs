/******************************************************************************
Copyright 2006 Palmsource, Inc (an ACCESS company).

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*****************************************************************************/

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "sqlfs.h"

char *data = "this is a string";

/* support functions -------------------------------------------------------- */

int exists(char *filename)
{
    FILE *file;
    if ((file = fopen(filename, "r")) == NULL) {
        if (errno == ENOENT) {
            return 0;
        } else {
            // Check for other errors too, like EACCES and EISDIR
            printf("Error occured: %d", errno);
        }
        return 0;
    } else {
        fclose(file);
    }
    return 1;
}

int create_test_file(sqlfs_t* sqlfs, char* filename, int size)
{
    int i;
    char randomdata[size];
    struct fuse_file_info fi = { 0 };
    fi.flags |= ~0;
    for (i=0; i<size; ++i)
        randomdata[i] = (i % 90) + 32;
    randomdata[size-1] = '\0';
    sqlfs_proc_write(sqlfs, filename, randomdata, size, 0, &fi);
}

void randomfilename(char* buf, int size, char* prefix)
{
    snprintf(buf, size, "/%s-random-%i", prefix, rand());
}


/* tests -------------------------------------------------------------------- */

void test_mkdir_with_sleep(sqlfs_t *sqlfs)
{
    printf("Testing mkdir with sleep...");
    char* testfilename = "/mkdir-with-sleep0";
    sqlfs_proc_mkdir(sqlfs, testfilename, 0777);
    sleep(1);
    assert(sqlfs_is_dir(sqlfs, testfilename));
    testfilename = "/mkdir-with-sleep1";
    sqlfs_proc_mkdir(sqlfs, testfilename, 0777);
    sleep(1);
    assert(sqlfs_is_dir(sqlfs, testfilename));
    printf("passed\n");
}

void test_mkdir_without_sleep(sqlfs_t *sqlfs)
{
    printf("Testing mkdir without sleep...");
    char* testfilename = "/mkdir-without-sleep0";
    sqlfs_proc_mkdir(sqlfs, testfilename, 0777);
    assert(sqlfs_is_dir(sqlfs, testfilename));
    printf("passed\n");

    printf("Testing whether mkdir does not make nested dirs...");
    testfilename = "/a/b/c/d/e/f/g";
    sqlfs_proc_mkdir(sqlfs, testfilename, 0777);
    assert(!sqlfs_is_dir(sqlfs, testfilename));
    printf("passed\n");
}

void test_mkdir_to_make_nested_dirs(sqlfs_t *sqlfs)
{
    printf("Testing mkdir to make nested dirs one at a time...");
    sqlfs_proc_mkdir(sqlfs, "/test", 0777);
    assert(sqlfs_is_dir(sqlfs, "/test"));
    sqlfs_proc_mkdir(sqlfs, "/test/1", 0777);
    assert(sqlfs_is_dir(sqlfs, "/test/1"));
    sqlfs_proc_mkdir(sqlfs, "/test/1/2", 0777);
    assert(sqlfs_is_dir(sqlfs, "/test/1/2"));
    printf("passed\n");
}

void test_rmdir(sqlfs_t *sqlfs)
{
    printf("Testing rmdir...");
    char* testfilename = "/mkdir-to-rmdir";
    sqlfs_proc_mkdir(sqlfs, testfilename, 0777);
    assert(sqlfs_is_dir(sqlfs, testfilename));
    sqlfs_proc_rmdir(sqlfs, testfilename);
    assert(!sqlfs_is_dir(sqlfs, testfilename));
    printf("passed\n");
}

void test_create_file_with_small_string(sqlfs_t *sqlfs)
{
    printf("Testing creating a file with a small string...");
    int i;
    char buf[200];
    struct fuse_file_info fi = { 0 };
    fi.flags |= ~0;
    sqlfs_proc_mkdir(sqlfs, "/bufdir", 0777);
    sqlfs_proc_write(sqlfs, "/bufdir/file", data, strlen(data), 0, &fi);
    assert(!sqlfs_is_dir(sqlfs, "/bufdir/file"));
    i = sqlfs_proc_read(sqlfs, "/bufdir/file", buf, sizeof(buf), 0, &fi);
    buf[i] = 0;
    assert(!strcmp(buf, data));
    printf("passed\n");
}

void test_write_n_bytes(sqlfs_t *sqlfs, int testsize)
{
    printf("Testing writing %d bytes of data...", testsize);
    int i;
    char testfilename[PATH_MAX];
    char randombuf[testsize];
    char randomdata[testsize];
    struct fuse_file_info fi = { 0 };
    randomfilename(testfilename, PATH_MAX, "write_n_bytes");
    for (i=0; i<testsize; ++i)
        randomdata[i] = (i % 90) + 32;
    randomdata[testsize-1] = 0;
    sqlfs_proc_write(sqlfs, testfilename, randomdata, testsize, 0, &fi);
    sleep(1);
    i = sqlfs_proc_read(sqlfs, testfilename, randombuf, testsize, 0, &fi);
    randombuf[i] = 0;
    assert(!strcmp(randombuf, randomdata));
    printf("passed\n");
}

void test_read_bigger_than_buffer(sqlfs_t *sqlfs)
{
    printf("Testing reading while requesting more bytes than will fit in the buffer...");
    int i;
    int bufsize = 200;
    int filesize = bufsize * 4;
    char testfilename[PATH_MAX];
    char buf[bufsize];
    struct fuse_file_info fi = { 0 };
    randomfilename(testfilename, PATH_MAX, "read_bigger_than_buffer");
    create_test_file(sqlfs, testfilename, filesize);
    i = sqlfs_proc_read(sqlfs, testfilename, buf, sizeof(buf), bufsize, &fi);
    assert(i==sizeof(buf));
    printf("passed\n");
}

void test_read_byte_with_offset(sqlfs_t *sqlfs, int testsize)
{
    printf("Testing reading a byte with offset 10000 times...");
    int i, y, readloc;
    char buf[200];
    char testfilename[PATH_MAX];
    struct fuse_file_info fi = { 0 };
    randomfilename(testfilename, PATH_MAX, "read_byte_with_offset");
    create_test_file(sqlfs, testfilename, testsize);
    fi.flags |= ~0;
    for(y=0; y<10000; y++)
    {
        readloc = (float)rand() / RAND_MAX * (testsize-1);
        i = sqlfs_proc_read(sqlfs, testfilename, buf, 1, readloc, &fi);
        assert(i == 1);
        assert(buf[0] == (readloc % 90) + 32);
    }
    printf("passed\n");
}

void test_truncate(sqlfs_t *sqlfs, int testsize)
{
    printf("Testing truncating...");
    struct stat sb;
    char testfilename[PATH_MAX];
    randomfilename(testfilename, PATH_MAX, "truncate");
    create_test_file(sqlfs, testfilename, testsize);
    sqlfs_proc_getattr(sqlfs, testfilename, &sb);
    assert(sb.st_size == testsize);
    sqlfs_proc_truncate(sqlfs, testfilename, 0);
    sqlfs_proc_getattr(sqlfs, testfilename, &sb);
    assert(sb.st_size == 0);
    printf("passed\n");
}

void test_truncate_existing_file(sqlfs_t *sqlfs, int testsize)
{
    printf("Testing opening existing file truncation...");
    char testfilename[PATH_MAX];
    struct stat sb;
    struct fuse_file_info ffi;
    randomfilename(testfilename, PATH_MAX, "truncate");
    create_test_file(sqlfs, testfilename, testsize);
    sqlfs_proc_getattr(sqlfs, testfilename, &sb);
    assert(sb.st_size == testsize);
    ffi.flags = O_WRONLY | O_CREAT | O_TRUNC;
    ffi.direct_io = 0;
    int rc = sqlfs_proc_open(sqlfs, testfilename, &ffi);
    assert(rc == 0);
    sqlfs_proc_getattr(sqlfs, testfilename, &sb);
    assert(sb.st_size == 0);
    printf("passed\n");
}

void test_write_seek_write(sqlfs_t *sqlfs)
{
    printf("Testing write/seek/write...");
    char* testfilename = "/skipwrite";
    char buf[200];
    const int skip_offset = 100;
    const char skip1[25] = "it was the best of times";
    const int sz_skip1 = strlen(skip1);
    struct stat sb;
    struct fuse_file_info fi = { 0 };
    fi.flags |= ~0;
    int rc = sqlfs_proc_write(sqlfs, testfilename, skip1, sz_skip1, 0, &fi);
    assert(rc);
    const char skip2[26] = "it was the worst of times";
    const int sz_skip2 = strlen(skip2);
    rc = sqlfs_proc_write(sqlfs, testfilename, skip2, sz_skip2, skip_offset, &fi);
    assert(rc);
    sqlfs_proc_getattr(sqlfs, testfilename, &sb);
    assert(sb.st_size == (skip_offset+sz_skip2));
    int i = sqlfs_proc_read(sqlfs, testfilename, buf, sz_skip1, 0, &fi);
    assert(i == sz_skip1);
    buf[sz_skip1] = 0;
    assert(!strcmp(buf, skip1));
    i = sqlfs_proc_read(sqlfs, testfilename, buf, sz_skip2, skip_offset, &fi);
    assert(i == sz_skip2);
    buf[sz_skip2] = 0;
    assert(!strcmp(buf, skip2));
    printf("passed\n");
}

void run_standard_tests(sqlfs_t* sqlfs)
{
    int size;

    test_mkdir_with_sleep(sqlfs);
    test_mkdir_without_sleep(sqlfs);
    test_mkdir_to_make_nested_dirs(sqlfs);
    test_rmdir(sqlfs);
    test_create_file_with_small_string(sqlfs);
    test_write_seek_write(sqlfs);
    test_read_bigger_than_buffer(sqlfs);

    for (size=10; size < 1000001; size *= 10) {
        test_write_n_bytes(sqlfs, size);
        test_read_byte_with_offset(sqlfs, size);
        test_truncate(sqlfs, size);
        test_truncate_existing_file(sqlfs, size);
    }
}