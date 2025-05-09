#pragma once

#include "lute/LuteLibs.h"

LUTE_DECLARE_TYPE(export type FileHandle = {
    read fd: number,
    read err: number
})

LUTE_DECLARE_TYPE(export type FileType = "unknown" | "file"| "dir" | "link" | "fifo" | "socket" | "char" | "block")

LUTE_DECLARE_TYPE(export type DirectoryEntry = {
    name: string,
    type: FileType
})

LUTE_DECLARE_FUNCTION(fs, open, (path: string, mode: string)->FileHandle);

LUTE_DECLARE_FUNCTION(fs, readfile, (path: string)->string);
LUTE_DECLARE_FUNCTION(fs, writefile, (path: string, contents: string)->());

LUTE_DECLARE_FUNCTION(fs, type, (path: string)->(FileType));
LUTE_DECLARE_FUNCTION(fs, listdir, (path: string)->({DirectoryEntry}));

LUTE_DEFINE_LIB(fs,
    LUTE_LIB_ENTRY(open)

    LUTE_LIB_ENTRY(readfile)
    LUTE_LIB_ENTRY(writefile)

    LUTE_LIB_ENTRY(type)
    LUTE_LIB_ENTRY(listdir)
)
