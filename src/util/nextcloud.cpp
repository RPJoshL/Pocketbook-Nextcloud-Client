//------------------------------------------------------------------
// nextcloud.cpp
//
// Author:           JuanJakobo
// Date:             04.08.2020
//
//-------------------------------------------------------------------

#include "inkview.h"
#include "nextcloud.h"
#include "util.h"
#include "item.h"
#include "log.h"

#include <string>
#include <curl/curl.h>
#include <fstream>
#include <sstream>

using std::ifstream;
using std::ofstream;
using std::string;

//neccesary to use Dialog method
Nextcloud *Nextcloud::nextcloudStatic;

Nextcloud::Nextcloud()
{
    nextcloudStatic = this;

    if (iv_access(NEXTCLOUD_PATH.c_str(), W_OK) != 0)
        iv_mkdir(NEXTCLOUD_PATH.c_str(), 0777);

    if (iv_access(NEXTCLOUD_FILE_PATH.c_str(), W_OK) != 0)
        iv_mkdir(NEXTCLOUD_FILE_PATH.c_str(), 0777);
}

void Nextcloud::setURL(const string &Url)
{
    iconfigedit *temp = nullptr;
    iconfig *nextcloudConfig = OpenConfig(NEXTCLOUD_CONFIG_PATH.c_str(), temp);
    WriteString(nextcloudConfig, "url", Url.c_str());
    CloseConfig(nextcloudConfig);
}

void Nextcloud::setUsername(const string &Username)
{
    iconfigedit *temp = nullptr;
    iconfig *nextcloudConfig = OpenConfig(NEXTCLOUD_CONFIG_PATH.c_str(), temp);
    WriteString(nextcloudConfig, "username", Username.c_str());
    CloseConfig(nextcloudConfig);
}

void Nextcloud::setPassword(const string &Pass)
{
    iconfigedit *temp = nullptr;
    iconfig *nextcloudConfig = OpenConfig(NEXTCLOUD_CONFIG_PATH.c_str(), temp);
    WriteSecret(nextcloudConfig, "password", Pass.c_str());
    CloseConfig(nextcloudConfig);
}

void Nextcloud::setStartFolder(const string &Path)
{
    iconfigedit *temp = nullptr;
    iconfig *nextcloudConfig = OpenConfig(NEXTCLOUD_CONFIG_PATH.c_str(), temp);
    WriteString(nextcloudConfig, "startFolder", Path.c_str());
    CloseConfig(nextcloudConfig);
}

bool Nextcloud::login()
{
    string tempPath = getStartFolder();

    if (tempPath.empty())
        tempPath = NEXTCLOUD_ROOT_PATH + this->getUsername() + "/";

    if (getDataStructure(tempPath, this->getUsername(), this->getPassword()))
    {
        _loggedIn = true;
        return true;
    }

    return false;
}

bool Nextcloud::login(const string &Url, const string &Username, const string &Pass)
{
    _url = Url;
    string tempPath = NEXTCLOUD_ROOT_PATH + Username + "/";
    if (getDataStructure(tempPath, Username, Pass))
    {
        if (iv_access(NEXTCLOUD_CONFIG_PATH.c_str(), W_OK) != 0)
            iv_buildpath(NEXTCLOUD_CONFIG_PATH.c_str());
        this->setUsername(Username);
        this->setPassword(Pass);
        this->setURL(_url);
        this->setStartFolder(tempPath);
        _loggedIn = true;
        return true;
    }
    return false;
}

void Nextcloud::logout(bool deleteFiles)
{
    if (deleteFiles)
    {
        string cmd = "rm -rf " + NEXTCLOUD_FILE_PATH + "/" + getUsername() + "/";
        system(cmd.c_str());
    }
    remove(NEXTCLOUD_CONFIG_PATH.c_str());
    remove((NEXTCLOUD_CONFIG_PATH + ".back.").c_str());

    _url.clear();
    _items = nullptr;
    _workOffline = false;
    _loggedIn = false;
}

void Nextcloud::downloadItem(int itemID)
{
    Log::writeLog("started download of " + _items->at(itemID).getPath() + " to " + _items->at(itemID).getLocalPath());

    if (!Util::connectToNetwork())
    {
        Message(ICON_WARNING, "Warning", "Can not connect to the Internet. Switching to offline modus.", 1200);
        _workOffline = true;
    }

    if (_items->at(itemID).getPath().empty())
    {
        Message(ICON_ERROR, "Error", "Download path is not set, therefore cannot download the file.", 1200);
        return;
    }

    UpdateProgressbar("Starting Download", 0);

    CURLcode res;
    CURL *curl = curl_easy_init();

    if (curl)
    {
        string post = this->getUsername() + std::string(":") + this->getPassword();

        FILE *fp;
        fp = iv_fopen(_items->at(itemID).getLocalPath().c_str(), "wb");

        curl_easy_setopt(curl, CURLOPT_URL, (_url + _items->at(itemID).getPath()).c_str());
        curl_easy_setopt(curl, CURLOPT_USERPWD, post.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Util::writeData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, false);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, Util::progress_callback);
        //Follow redirects
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        iv_fclose(fp);

        if (res == CURLE_OK)
        {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            switch (response_code)
            {
            case 200:
                Log::writeLog("finished download of " + _items->at(itemID).getPath() + " to " + _items->at(itemID).getLocalPath());
                _items->at(itemID).setState(FileState::ISYNCED);
                break;
            case 401:
                Message(ICON_ERROR, "Error", "Username/password incorrect.", 1200);
                break;
            default:
                Message(ICON_ERROR, "Error", ("An unknown error occured. (Curl Response Code " + Util::valueToString(response_code) + ")").c_str(), 1200);
                break;
            }
        }
    }
}

bool Nextcloud::getDataStructure(string &pathUrl)
{
    return getDataStructure(pathUrl, this->getUsername(), this->getPassword());
}

bool Nextcloud::getDataStructure(const string &pathUrl, const string &Username, const string &Pass)
{
    //could not connect to internet, therefore offline modus
    if (_workOffline)
        return getOfflineStructure(pathUrl);

    if (!Util::connectToNetwork())
    {
        Message(ICON_WARNING, "Warning", "Cannot connect to the internet. Switching to offline modus. To work online turn on online modus in the menu.", 200);
        _workOffline = true;
        return getOfflineStructure(pathUrl);
    }

    if (_url.empty())
        _url = this->getUrl();

    if (Username.empty() || Pass.empty())
    {
        Message(ICON_ERROR, "Error", "Username/password not set.", 1200);
        return false;
    }

    string readBuffer;
    CURLcode res;
    CURL *curl = curl_easy_init();

    if (curl)
    {
        string post = Username + ":" + Pass;

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Depth: 1");
        curl_easy_setopt(curl, CURLOPT_URL, (_url + pathUrl).c_str());
        curl_easy_setopt(curl, CURLOPT_USERPWD, post.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Util::writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK)
        {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            switch (response_code)
            {
            case 207:
            {
                string localPath = this->getLocalPath(pathUrl);

                //create items_
                if (!readInXML(readBuffer))
                    return false;

                if (iv_access(localPath.c_str(), W_OK) != 0)
                {
                    //if the current folder does not exist locally, create it
                    iv_buildpath(localPath.c_str());
                }

                else
                {
                    //get items from local path
                    if (iv_access(localPath.c_str(), R_OK) != 0)
                    {
                        Log::writeLog("Local structure of " + localPath + " found.");
                        //TODO if has files that are not online, add to _items
                    }

                    getLocalFileStructure(localPath);
                }

                //TODO structure as CSV?
                //update the .structure file acording to items in the folder
                localPath = localPath + NEXTCLOUD_STRUCTURE_EXTENSION;

                //save xml to make the structure available offline
                ofstream outFile(localPath);
                if (outFile)
                {
                    outFile << readBuffer;
                    Log::writeLog("Saved local copy of tree structure to " + localPath);
                }
                else
                {
                    Log::writeLog(localPath + "Couldnt save copy of tree structure locally.");
                }

                outFile.close();
                return true;
            }
            case 401:
                Message(ICON_ERROR, "Error", "Username/password incorrect.", 1200);
                break;
            default:
                Message(ICON_ERROR, "Error", ("An unknown error occured. Switching to offline modus. To work online turn on online modus in the menu. (Curl Response Code " + Util::valueToString(response_code) + ")").c_str(), 1200);
                _workOffline = true;
                return getOfflineStructure(pathUrl);
            }
        }
    }
    return false;
}

string Nextcloud::getUrl()
{
    iconfigedit *temp = nullptr;
    iconfig *nextcloudConfig = OpenConfig(NEXTCLOUD_CONFIG_PATH.c_str(), temp);
    string url = ReadString(nextcloudConfig, "url", "");
    CloseConfigNoSave(nextcloudConfig);
    return url;
}

string Nextcloud::getUsername()
{
    iconfigedit *temp = nullptr;
    iconfig *nextcloudConfig = OpenConfig(NEXTCLOUD_CONFIG_PATH.c_str(), temp);
    string user = ReadString(nextcloudConfig, "username", "");
    CloseConfigNoSave(nextcloudConfig);
    return user;
}

string Nextcloud::getPassword()
{
    iconfigedit *temp = nullptr;
    iconfig *nextcloudConfig = OpenConfig(NEXTCLOUD_CONFIG_PATH.c_str(), temp);
    string pass = ReadSecret(nextcloudConfig, "password", "");
    CloseConfigNoSave(nextcloudConfig);
    return pass;
}

string Nextcloud::getStartFolder()
{
    iconfigedit *temp = nullptr;
    iconfig *nextcloudConfig = OpenConfig(NEXTCLOUD_CONFIG_PATH.c_str(), temp);
    string startFolder = ReadString(nextcloudConfig, "startFolder", "");
    CloseConfigNoSave(nextcloudConfig);
    return startFolder;
}

void Nextcloud::DialogHandlerStatic(int Button)
{
    if (Button == 2)
    {
        nextcloudStatic->_workOffline = true;
    }
}

bool Nextcloud::readInXML(string xml)
{
    size_t begin;
    size_t end;
    string beginItem = "<d:response>";
    string endItem = "</d:response>";
    vector<Item> tempItems;

    begin = xml.find(beginItem);

    while (begin != std::string::npos)
    {
        end = xml.find(endItem);

        tempItems.push_back(Item(xml.substr(begin, end)));

        xml = xml.substr(end + endItem.length());

        begin = xml.find(beginItem);
    }

    if (_items)
        _items->clear();
    _items = std::make_shared<vector<Item>>(std::move(tempItems));

    if (_items->size() < 1)
        return false;

    //resize item 1
    string header = _items->at(0).getPath();
    header = header.substr(0, header.find_last_of("/"));
    header = header.substr(0, header.find_last_of("/") + 1);
    _items->at(0).setPath(header);
    _items->at(0).setTitle("...");
    _items->at(0).setLastEditDate("");

    if (_items->at(0).getPath().compare(NEXTCLOUD_ROOT_PATH) == 0)
        _items->erase(_items->begin());

    return true;
}

string Nextcloud::getLocalPath(string path)
{
    Util::decodeUrl(path);
    if (path.find(NEXTCLOUD_ROOT_PATH) != string::npos)
        path = path.substr(NEXTCLOUD_ROOT_PATH.length());

    return NEXTCLOUD_FILE_PATH + "/" + path;
}

bool Nextcloud::getOfflineStructure(const string &pathUrl)
{
    string localPath = this->getLocalPath(pathUrl) + NEXTCLOUD_STRUCTURE_EXTENSION;
    if (iv_access(localPath.c_str(), W_OK) == 0)
    {
        ifstream inFile(localPath);
        std::stringstream buffer;
        buffer << inFile.rdbuf();

        if (!readInXML(buffer.str()))
            return false;

        getLocalFileStructure(this->getLocalPath(pathUrl));
    }
    else
    {
        if (pathUrl.compare(NEXTCLOUD_ROOT_PATH + getUsername() + "/") == 0)
        {
            Message(ICON_ERROR, "Error", "The root structure is not available offline. Please try again to login.", 1200);
            logout();
        }
        else
        {
            //Structure is not available offline, stay at the tree
            Message(ICON_ERROR, "Error", "The selected structure is not available offline.", 1200);
        }
    }

    return false;
}

void Nextcloud::getLocalFileStructure(const string &localPath)
{
    //TODO also get folders
    //get local files, https://stackoverflow.com/questions/306533/how-do-i-get-a-list-of-files-in-a-directory-in-c
    DIR *dir;
    class dirent *ent;
    class stat st;

    dir = opendir(localPath.c_str());
    while ((ent = readdir(dir)) != NULL)
    {
        const string file_name = ent->d_name;
        const string full_file_name = localPath + file_name;

        if (file_name[0] == '.')
            continue;

        if (stat(full_file_name.c_str(), &st) == -1)
            continue;

        //also include directory
        const bool is_directory = (st.st_mode & S_IFDIR) != 0;
        if (is_directory)
            continue;

        bool found = false;
        for (auto i = 0; i < _items->size(); i++)
        {
            //TODO compare last edit local and in cloud and display to user
            if (_items->at(i).getLocalPath().compare(full_file_name) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            _items->push_back(Item(full_file_name, FileState::ILOCAL));
        }
    }
    closedir(dir);
}
