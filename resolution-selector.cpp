#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <math.h>

struct Mode
{
    int width = 0;
    int height = 0;
    int pixels = 0;
    float aspectRatio = 0;
    float refreshRate = 0;
};

struct Monitor
{
    char connector[64] = "";
    char mode[64] = "";

    int x = 0;
    int y = 0;

    float scale = 1.0f;

    char transform[32] = "normal";

    bool primary = false;
};

int StringToInt(char s[8])
{
    int n = 0;
    for (int i = 0; i < strlen(s); i++)
    {
        if (s[i] < '0' || s[i] > '9')
            return -1;

        n = n * 10 + (s[i] - '0');
    }
    return n;
}

float StringToFloat(char s[8])
{
    float n = 0, d = 0;
    int decimalPointPos;

    if (strchr(s, '.') == NULL)
    {
        n = StringToInt(s);
        if (n < 0 || n > 999)
            return -1;

        return n;
    }

    decimalPointPos = strchr(s, '.') - s;
    if (strlen(s) - decimalPointPos < 2 || strlen(s) - decimalPointPos > 4)
        return -1;

    char intPart[8];
    strncpy(intPart, s, decimalPointPos);
    intPart[decimalPointPos] = '\0';
    n = StringToInt(intPart);
    if (n == -1)
        return -1;

    d = StringToInt(s + decimalPointPos + 1);
    if (d == -1)
        return -1;

    if (d < 10)
        d = d / 10.0;
    else if (d < 100)
        d = d / 100.0;
    else if (d < 1000)
        d = d / 1000.0;
    n = n + d;

    return n;
}

int main(int argc, char *argv[])
{
    // Get Input (Target resolution)
    if (argc != 4 && argc != 5)
    {
        std::cout << "Invalid number of arguments:" << std::endl
                  << "Please specify the targeted resolution (<monitor_id> <width> <height>)"
                  << " or (<monitor_id> <width> <height> <refresh_rate>)." << std::endl;
        return 0;
    }

    Mode target;
    bool ok = true;

    if (strlen(argv[2]) > 4 || strlen(argv[3]) > 4)
        ok = false;

    target.width = StringToInt(argv[2]);
    if (target.width == -1)
        ok = false;

    target.height = StringToInt(argv[3]);
    if (target.height == -1)
        ok = false;

    if (argc == 5)
    {
        if (strlen(argv[4]) > 7)
            ok = false;

        target.refreshRate = StringToFloat(argv[4]);
        if (target.refreshRate == -1)
            ok = false;
    }
    else
        target.refreshRate = -1;

    if (!ok)
    {
        std::cout << "Invalid resolution or refresh rate or missing monitor id:"
                  << std::endl
                  << "Monitor ID is expected to be the first argument and is mandatory."
                  << std::endl
                  << "Width and Height are expected to be positive integers smaller 10000."
                  << std::endl
                  << "Refresh rate (if specified) is expected to be a positive "
                  << "real number smaller than 1000 and with maximum 3 decimals."
                  << std::endl;
        return 0;
    }

    target.pixels = target.width * target.height;
    target.aspectRatio = float(target.width) / float(target.height);
    std::cout << "Target resolution: " << target.width << 'x' << target.height << std::endl;
    if (target.refreshRate != -1)
        std::cout << "Target refresh rate: " << target.refreshRate << std::endl;
    std::cout << std::endl;

    // Get available resolutions
    char path[2048], filePath[2048], command[2088], monitor[256], rl[2048], *q;
    Mode availableResolution[1024];
    int foundResolutions = 0;

    strcpy(path, argv[0]);
    q = path;
    while (strchr(q, '/'))
        q = strchr(q, '/') + 1;
    strcpy(q, "\0");
    strcpy(filePath, path);
    strcat(filePath, "monitorModes.txt");
    // std::cout << path << std::endl;

    strcpy(command, "gdctl show -m > \"");
    strcat(command, filePath);
    strcat(command, "\"");
    // std::cout << command << std::endl;

    system(command);

    strcpy(monitor, "Monitor ");
    strcat(monitor, argv[1]);

    std::ifstream fin(filePath);
    strcpy(rl, "");
    while (!fin.eof() && strstr(rl, monitor) == NULL)
        fin.getline(rl, 2048);

    if (fin.eof())
    {
        std::cout << monitor << " not found." << std::endl;
        fin.close();
        return 0;
    }

    while (strstr(rl, "Modes") == NULL)
        fin.getline(rl, 2048);
    std::cout << monitor << " available resolutions: " << std::endl;
    strcpy(rl, "");
    fin.getline(rl, 2048);
    while (strstr(rl, "Preferences") == NULL)
    {
        Mode res;
        int i = 0, j;
        char w[8] = "", h[8] = "", rr[8] = "";

        while (rl[i] < '0' || rl[i] > '9')
            i++;
        j = 0;
        while (rl[i] != 'x')
            w[j++] = rl[i++];
        i++;
        j = 0;
        while (rl[i] != '@')
            h[j++] = rl[i++];
        i++;
        j = 0;
        while (rl[i] != '\0')
            rr[j++] = rl[i++];

        res.width = StringToInt(w);
        res.height = StringToInt(h);
        res.refreshRate = StringToFloat(rr);

        std::cout << "---- " << res.width << 'x' << res.height << ' ' << res.refreshRate << "Hz";

        fin.getline(rl, 2048);

        if (res.width == -1 || res.height == -1 || res.refreshRate == -1)
        {
            std::cout << " Invalid resolution listed (it will not be included)" << std::endl;
            continue;
        }
        std::cout << std::endl;

        res.pixels = res.width * res.height;
        res.aspectRatio = float(res.width) / float(res.height);
        availableResolution[foundResolutions++] = res;
    }

    std::cout << "Found " << foundResolutions << " resolutions available." << std::endl
              << std::endl;
    fin.close();

    // Chose the closest resolution
    Mode closest = availableResolution[0];
    int removed = 0;
    float minDifference = abs(closest.aspectRatio - target.aspectRatio);

    for (int i = 1; i < foundResolutions; i++)
        if (abs(availableResolution[i].aspectRatio - target.aspectRatio) < minDifference)
            minDifference = abs(availableResolution[i].aspectRatio - target.aspectRatio);
    for (int i = 0; i < foundResolutions; i++)
    {
        availableResolution[i - removed] = availableResolution[i];
        if (abs(availableResolution[i].aspectRatio - target.aspectRatio) != minDifference)
            removed++;
    }
    foundResolutions -= removed;

    /*
    std::cout << "Filtred resolutions: " << std::endl;
    for (int i = 0; i < foundResolutions; i++)
        std::cout << "---- " << availableResolution[i].width << 'x'
                  << availableResolution[i].height << ' ' << availableResolution[i].refreshRate
                  << "Hz " << availableResolution[i].aspectRatio << ' ' << availableResolution[i].pixels << std::endl;
    std::cout << "Remaining " << foundResolutions << " resolutions." << std::endl;
    */

    closest = availableResolution[0];
    removed = 0;
    minDifference = abs(closest.pixels - target.pixels);
    for (int i = 1; i < foundResolutions; i++)
        if (abs(availableResolution[i].pixels - target.pixels) < minDifference)
            minDifference = abs(availableResolution[i].pixels - target.pixels);
    for (int i = 0; i < foundResolutions; i++)
    {
        availableResolution[i - removed] = availableResolution[i];
        if (abs(availableResolution[i].pixels - target.pixels) != minDifference)
            removed++;
    }
    foundResolutions -= removed;

    /*
    std::cout << "Filtred resolutions: " << std::endl;
    for (int i = 0; i < foundResolutions; i++)
        std::cout << "---- " << availableResolution[i].width << 'x'
                  << availableResolution[i].height << ' ' << availableResolution[i].refreshRate
                  << "Hz" << ' ' << availableResolution[i].pixels << std::endl;
    std::cout << "Remaining " << foundResolutions << " resolutions." << std::endl;
    */

    if (target.refreshRate != -1)
    {
        closest = availableResolution[0];
        removed = 0;
        minDifference = abs(closest.refreshRate - target.refreshRate);
        for (int i = 1; i < foundResolutions; i++)
            if (abs(availableResolution[i].refreshRate - target.refreshRate) < minDifference)
                minDifference = abs(availableResolution[i].refreshRate - target.refreshRate);
        for (int i = 0; i < foundResolutions; i++)
        {
            availableResolution[i - removed] = availableResolution[i];
            if (abs(availableResolution[i].refreshRate - target.refreshRate) != minDifference)
                removed++;
        }
        foundResolutions -= removed;
    }

    std::cout << "Filtred resolutions: " << std::endl;
    for (int i = 0; i < foundResolutions; i++)
        std::cout << "---- " << availableResolution[i].width << 'x'
                  << availableResolution[i].height << ' ' << availableResolution[i].refreshRate
                  << "Hz" << std::endl;
    std::cout << "Remaining " << foundResolutions << " resolutions." << std::endl
              << std::endl;

    closest = availableResolution[0];
    removed = 0;
    for (int i = 1; i < foundResolutions; i++)
        if (availableResolution[i].pixels > closest.pixels)
            closest = availableResolution[i];
    for (int i = 1; i < foundResolutions; i++)
    {
        availableResolution[i - removed] = availableResolution[i];
        if (availableResolution[i].pixels != closest.pixels)
            removed++;
    }
    foundResolutions -= removed;

    closest = availableResolution[0];
    removed = 0;
    for (int i = 1; i < foundResolutions; i++)
        if (availableResolution[i].refreshRate > closest.refreshRate)
            closest = availableResolution[i];

    std::cout << "Selected resolution: " << closest.width << 'x' << closest.height << ' '
              << closest.refreshRate << "Hz." << std::endl;

    // Apply Configuration
    strcpy(filePath, path);
    strcat(filePath, "selectedMode.txt");
    std::ofstream fout(filePath);
    fout << closest.width << 'x' << closest.height << '@' << closest.refreshRate;
    fout.close();
    std::ifstream fin1(filePath);
    char mode[32];
    fin1 >> mode;
    fin1.close();

    strcpy(command, "gdctl set --logical-monitor --primary --monitor ");
    strcat(command, argv[1]);
    strcat(command, " --mode ");
    strcat(command, mode);

    std::cout << command << std::endl;
    system(command);

    return 0;
}