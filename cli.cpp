/*
    Dimension
    (c) LifesMinder

    Module name:
        cli.cpp
    Purpose:
        Command-line commands management file
*/

#include "dimension.h"

extern map<int, wchar_t> flags;
extern map<wstring, any> options;

void CLI() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int realArgc = argc - 1;

    if (argv != NULL && realArgc > 0) {
        // Attach the console
        FILE* f;
        if (!AttachConsole(ATTACH_PARENT_PROCESS))
            AllocConsole();
        freopen_s(&f, "CONOUT$", "w", stdout);

        // Traverse through the arguments.
        // argv[0] is executable, real args start at 1
        for (int i = 1; i < argc; i++) {
            int index = i - 1;
            wstring arg = argv[i];

            if (arg[0] == L'-') {
                if (arg.size() > 1 && arg[1] == L'-') {
                    wstring key = arg.substr(2);
                    options.emplace(key, L"");
                }
                else if (arg.size() > 1) {
                    flags.emplace(index, arg[1]);
                }
            }
            else if (arg[0] == L'/') {
                size_t colon = arg.find(L":");
                wstring key = arg.substr(1, colon - 1), value = L"";
                if (colon != wstring::npos) {
                    value = arg.substr(colon + 1);
                }
                options.emplace(key, value);
            }
        }

        // Work with options. To-Do: separate this exact logic
        if (options.find(L"debug") != options.end()) {
            cout << endl;
            cout << "[Dimension] Debug mode is on" << endl;
        }

        FreeConsole();
        LocalFree(argv);
        fclose(f);
    }
}