## FUSE Filesystem that has all basic fucntionalities  
- Read and write from files larger than one block. For example, supports one hundred 1k files or five 100k files.  
- Create directories and nested directories. Directory depth is limited only by disk space (and possibly the POSIX API).  
- Remove directories.  
- Hard links.  
- Symlinks  
- Support metadata (permissions and timestamps) for files and directories.  
