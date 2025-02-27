//------------------------------------------------------------------
// loginView.h
//
// Author:           JuanJakobo
// Date:             26.08.2020
// Description:
//-------------------------------------------------------------------

#ifndef LOGIN_SCREEN
#define LOGIN_SCREEN

#include "inkview.h"

#include <string>
#include <memory>

enum class KeyboardTarget
{
    IURL,
    IUSERNAME,
    IPASSWORD
};

const int KEYBOARD_STRING_LENGHT = 90;

class LoginView
{
public:
    /**
        * Draws the loginView includin URL, Username and Password buttons inside the contentRect
        *
        * @param contentRect area where the loginscreen shall be drawn
        */
    LoginView(const irect &contentRect);

    ~LoginView();

    /**
        * Checks which part of the loginscreen is shown and reacts accordingly
        *
        * @param x x-coordinate
        * @param y y-coordinate
        * @return int if event has been handled. Returns 2 if login has been clicked and all items are set
        */
    int logginClicked(int x, int y);

    std::string getUsername() { return _username; };
    std::string getPassword() { return _password; };
    std::string getURL() { return _url; };
    bool getIgnoreCert() { return _ignoreCert; };

private:
    static std::unique_ptr<LoginView> _loginViewStatic;
    int _loginFontHeight;
    ifont *_loginFont;
    const irect _contentRect;
    irect _urlButton;
    irect _loginButton;
    irect _usernameButton;
    irect _passwordButton;
    irect _ignoreCertButton;
    KeyboardTarget _target;
    std::string _username;
    std::string _password;
    std::string _url;
    bool _ignoreCert = false;
    std::string _temp;

    /**
        * Functions needed to call C function, handles the keyboard
        *
        * @param  text text that has been typed in by the user
        */
    static void keyboardHandlerStatic(char *text);

    /**
        * Called by the static method and saves and writes the input from the user to the screen
        *
        * @param text text that has been typed in by the user
        */
    void keyboardHandler(char *text);
};

#endif
