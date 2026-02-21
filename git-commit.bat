@echo off
setlocal

:: 1. Check if a commit message was provided
if "%~1" == "" (
    echo Error: No commit message provided.
    echo Usage: git-commit.bat "Your commit message"
    exit /b 1
)

:: Store the argument
set COMMIT_MSG=%~1

:: 2. Stage all changes
echo Staging changes...
git add .

:: 3. Commit changes
echo Committing changes...
git commit -m "%COMMIT_MSG%"

:: 4. Push to the current branch
:: This uses 'HEAD' to ensure it pushes the branch you are currently on
echo Pushing to remote...
git push origin HEAD

echo.
echo Done! Changes committed and pushed successfully.
endlocal