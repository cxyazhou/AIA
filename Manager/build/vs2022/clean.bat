@echo off
chcp 65001 >nul

:: Set file extensions to delete (space separated)
set "extensions=.recipe .exp .iobj .obj .ipdb .log .pdb .lib .tlog .txt"

echo Will delete files and directories with extensions: %extensions%
echo Current directory: %cd%

echo.
echo Starting deletion process...

set file_count=0
set dir_count=0

:: Delete directories with specified extensions
for %%e in (%extensions%) do (
    for /f "delims=" %%d in ('dir /b /ad *%%e 2^>nul') do (
        if exist "%%d" (
            echo Deleting DIRECTORY: %%d
            rd /s /q "%%d" >nul 2>&1
            set /a dir_count+=1
        )
    )
)

:: Delete files with specified extensions
for %%e in (%extensions%) do (
    for /f "delims=" %%f in ('dir /b *%%e 2^>nul') do (
        if exist "%%f" (
            echo Deleting FILE: %%f
            del "%%f" >nul 2>&1
            set /a file_count+=1
        )
    )
)

echo.
echo Deletion completed!
echo Files deleted: %file_count%
echo Directories deleted: %dir_count%
echo Total items deleted: %file_count% + %dir_count%
pause