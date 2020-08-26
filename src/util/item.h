//------------------------------------------------------------------
// item.h
//
// Author:           JuanJakobo
// Date:             04.08.2020
// Description:      Describes an WEBDAV item
//-------------------------------------------------------------------

#ifndef ITEM
#define ITEM

#include "inkview.h"

#include <string>

using std::string;

enum Itemtype
{
    IFILE,
    IFOLDER
};

class Item
{
public:
    Item(const string &xmlItem);

    void setPath(const string &path) { _path = path; };
    string getPath() const { return _path; };

    Itemtype getType() const { return _type; };

    void setTitle(const string &title) { _title = title; };
    string getTitle() const { return _title; };

    bool isDownloaded() const { return _downloaded; };

    string getLastEditDate() const { return _lastEditDate; };

    int getSize() const { return _size; };

    string getFiletype() const { return _fileType; };

    /**
        * downloads a file from WEBDAV and saves it 
        *         
        * @return true - sucessfull, false - error
        */
    string isClicked();

private:
    string _path;
    Itemtype _type;
    string _title;
    bool _downloaded;
    string _lastEditDate;
    int _size;
    string _fileType;
};
#endif