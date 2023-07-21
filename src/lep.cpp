#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <ios>
#include <iostream>
#include <string>
#include <unistd.h>
#include <stdlib.h> 
#include <fstream>
#include <termios.h>
#include <vector>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <sys/ioctl.h>
#include "../include/ModeState.h"
#include "../include/stev.h"
//#include <conio.h>

using namespace std;

//Original terminal settings 
struct termios defSet;

enum sKey {
  UP_KEY = 400,
  LEFT_KEY,
  RIGHT_KEY,
  DOWN_KEY,
  DEL_KEY,
};

struct textArea {
  int bX, bY;
  int eX, eY;

  textArea() {
    clear();
  }

  void clear() {
    bX = -1;
    bY = -1;
    eX = -1;
    eY = -1;
  }

  bool isNull() {
      return (bX == -1 || bY == -1 || eX == -1 || bY == -1);
  }
};

class Lep : public ModeState {
  public:
    Lep() {
      unsaved = false;
      cX = 0;
      cY = 0;
      getWinSize();
      readConfig();
    }

    void modeNormal() {
      int c;
      renderShownText(firstShownLine);
      updateStatBar();
      while(currentState == State::NORMAL) {
        c = readKey();
        //printf("%d ('%c')\n", c, c);
        switch (c) {
          case 27:
            handleEvent(Event::BACK);
            break;
          case 'i':
            handleEvent(Event::INPUT);
            break;
          case ':':
            handleEvent(Event::COMMAND);
            break;
          case 'v':
            handleEvent(Event::VISUAL);
            break;
          case 'V':
            handleEvent(Event::VLINE);
            break;
          case 'p':
            pasteClipboard();
            break;

          //Movement
          case 'h': case 'j': case 'k': case 'l':
          case LEFT_KEY: case DOWN_KEY: case UP_KEY: case RIGHT_KEY:
            curMove(c);
            break;
        }
      }
    }

    void modeInput() {
      int c;
      while (currentState == State::INPUT) {
        updateStatBar();
        c = readKey();
        switch (c) {
          case 27: //Esc
            handleEvent(Event::BACK);
            break;
          case 10: //Enter
            unsaved = true;
            newlineEscSeq();
            break;
          case 9: //Tab
            unsaved = true;
            tabKey();
            break;
          case 127: //Backspace
            unsaved = true;
            backspaceKey();
            break;
          case DEL_KEY: //Delete
            unsaved = true;
            deleteKey();
            break;

          //Movement
          case LEFT_KEY: case DOWN_KEY: case UP_KEY: case RIGHT_KEY:
            curMove(c);
            break;

          default:
            if (!iscntrl(c)) {
              unsaved = true;
              inputChar(c);
            }
            break;
        }
      }
    }

    void modeCommand() {
      int c;
      int comInd = oldCommands.size();
      string curCom = "";
      comLineText = ":";
      updateStatBar();
      //cout << "\033[s";
      while (currentState == State::COMMAND) {
        updateCommandBar();
        c = readKey();

        switch (c) {
          case 27:
            handleEvent(Event::BACK);
            //cout << "\033[u";
            break;
          case 127: //Backspace
            comModeDelChar();
            break;
          case 10: //Enter
            if (execCommand()) {
              oldCommands.push_back(comLineText);
            }
            handleEvent(Event::BACK);
            break;
          case UP_KEY:
            if (curCom == "") curCom = comLineText;
            if (comInd > 0) {
              comInd--;
              comLineText = oldCommands[comInd];
            }
            break;
          case DOWN_KEY:
            if (comInd < oldCommands.size()) {
              comInd++;
            }
            if (comInd == oldCommands.size()) {
              comLineText = curCom;
              break;
            }
            comLineText = oldCommands[comInd];

            break;
          default:
            if (!iscntrl(c) && c < 127) {
              comLineText += c;
              curCom = comLineText;
              comInd = oldCommands.size();
            }
            break;
        }
        if (comLineText.empty()) {
          handleEvent(Event::BACK);
        }
      }
    }

    void modeVisual() {
      if (currentState == State::VISUAL) {
        selectedText.bX = cX;
        selectedText.bY = cY;
        selectedText.eX = cX;
        selectedText.eY = cY;
      }
      else if (currentState == State::VLINE) {
        selectedText.bX = 0;
        selectedText.bY = cY;
        selectedText.eX = lines[cY].length();
        selectedText.eY = cY;
      }

      updateStatBar();
      int c;
      while (currentState == State::VISUAL || currentState == State::VLINE) {
        updateShowSelection();
        c = readKey();
        switch (c) {
          case 27: //Esc
            selectedText.clear();
            handleEvent(Event::BACK);
            break;
          case 10: //Enter
            break;
          case 127: //Backspace
            break;
          case DEL_KEY: //Delete
            deleteSelection();
            break;
          case 'y':
            copySelection();
            handleEvent(Event::BACK);
            break;
          case 'd':
            copySelection();
            deleteSelection();
            handleEvent(Event::BACK);
            break;
          case 'p':
            if (!clipboard.empty()) {
              deleteSelection();
              cX--;
              pasteClipboard();
            }
            handleEvent(Event::BACK);
            break;

          //Movement
          case 'h': case 'j': case 'k': case 'l':
          case LEFT_KEY: case DOWN_KEY: case UP_KEY: case RIGHT_KEY:
            curMove(c);
            updateSelectedText();
            break;

          default:
            break;
        }
      }
    }

    void readFile(string fName) {
      fileName = fName;
      fstream file(fileName);

      if(file) {
        cout << "File found" << endl;
      }
      else {
        cout << "File " << fileName << " not found" << endl;
        ofstream file(fileName);
        cout << "File " << fileName << " created" << endl;
        file.open(fileName, ios::in);
      }

      if (file.peek() == EOF) {
        lines.push_back("");
      } else {
        string line;
        while (getline(file, line)) {
          lines.push_back(line);
        }
      }
      file.close();

      sleep(1);
      clsResetCur();
    }
  
    void overwriteFile() {
      if (lines.empty() || (lines.size() == 1 && lines.front().empty())) {
        unsaved = false;
        return;
      } 
      ofstream file(fileName);
      for(const string& line : lines) {
        file << line << '\n';
      }
      file.close();
      unsaved = false;
    }

  private:
    int marginLeft = 7;
    int marginTop = 1;
    int winRows;
    int winCols;
    int firstShownLine = 0;
    vector<string> lines;
    bool unsaved;
    vector<char> validCommands = {'w', 'q'};
    string comLineText = "";
    vector<string> oldCommands;
    vector<string> clipboard;
    textArea selectedText;
    int cX, cY;

    string fileName;
    string configFilePath = "./.lepconfig";

    //Config
    int indentAm = 2;
    bool curWrap = true;
    int lineNumPad = 3;
    bool lineNum = true;
    bool relativenumber = false;
    int sBarColorBG;
    int sBarColorFG;

    void readConfig() {
      if (!lineNum) marginLeft = 2;
    }

    void inputChar(char c) {
      lines[cY].insert(cX, 1, c);
      cout << "\033[1C" << flush;
      cX++;
      updateRenderedLines(cY, 1);
    }

    void newlineEscSeq() {
      string newLine = lines[cY].substr(cX);
      lines[cY].erase(cX);
      cY++;
      lines.insert(lines.begin() + cY, newLine);
      cX = 0;
      if (cY-1 == firstShownLine + winRows - 2 - marginTop) {
        scrollDown();
        printf("\033[%dG", marginLeft);
      } else {
        printf("\033[1E\033[%dG", marginLeft);
      }
      fflush(stdout);
      updateRenderedLines(cY-1);
    }

    void tabKey() {
      lines[cY].insert(cX, string(indentAm, ' '));
      cX += indentAm;
      printf("\033[%dC", indentAm);
      fflush(stdout);
      updateRenderedLines(cY, 1);
    }

    void backspaceKey() {
      int ePos;
      if (cX == 0) {
        if (cY == 0) return;
        string delLine = lines[cY];
        lines.erase(lines.begin() + cY, lines.begin() + cY + 1);
        cY--;
        cX = lines[cY].length();
        lines[cY].append(delLine);
        printf("\033[1A\033[%dG", cX + marginLeft);
        updateRenderedLines(cY);
      } else {
        lines[cY].erase(cX - 1, 1);
        cX--;
        printf("\033[1D");
        updateRenderedLines(cY, 1);
      }
      fflush(stdout);
    }

    void deleteKey() {
      if (cX == lines[cY].length()) {
        if (cY == lines.size()-1) return;
        string delLine = lines[cY+1];
        lines.erase(lines.begin() + cY + 1);
        lines[cY] += delLine;
        updateRenderedLines(cY);
      } else {
        lines[cY].erase(cX, 1);
        updateRenderedLines(cY, 1);
      }
    }

    void comModeDelChar() {
      if (!comLineText.empty()) {
        comLineText.pop_back();
      }
    }

    int readKey() {
      int ret;
      char c;
      while ((ret = read(STDIN_FILENO, &c, 1)) != 1) {
        if (ret == -1) return '\0';
      }

      if (c == '\x1b') {
        char keySeq[3];
        if (read(STDIN_FILENO, &keySeq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &keySeq[1], 1) != 1) return '\x1b';

        if (keySeq[0] == '[') {

          if (keySeq[1] >= '0' && keySeq[1] <= '9') {
            if (read(STDIN_FILENO, &keySeq[2], 1) != 1) return '\x1b';
            if (keySeq[2] == '~') {
              switch (keySeq[1]) {
                case '3': return DEL_KEY;
              }
            }
          }
          else {
            switch (keySeq[1]) {
              case 'A': return UP_KEY;
              case 'B': return DOWN_KEY;
              case 'C': return RIGHT_KEY;
              case 'D': return LEFT_KEY;
            }
          }
        }
        return '\x1b';
      }
      else if (c < 0) {
        return '\0';
      } else {
        return c;
      }
    }

    void curMove(int c) {
      if (c == 'h' || c == LEFT_KEY) {
        curLeft();
      } else if (c == 'j' || c == DOWN_KEY) {
        curDown();
      } else if (c == 'k' || c == UP_KEY) {
        curUp();
      } else if (c == 'l' || c == RIGHT_KEY) {
        curRight();
      }
      updateStatBar();
      updateLineNums(firstShownLine);
    }

    void curUp() {
      if (lines.empty()) {
        return;
      }
      if (cY > 0) {
        if (cY == firstShownLine) {
          scrollUp();
        } else {
          cout << "\033[1A" << flush;
        }
        cY--;

        if (lines[cY].length() < cX) {
          printf("\033[%dD", cX - static_cast<int>(lines[cY].length()));
          cX = lines[cY].length();
        }
      }
      else {
        cX = 0;
        printf("\033[%d;%dH", cY + marginTop, cX + marginLeft);
      }
    }

    void curDown() {
      if (lines.empty()) {
        return;
      }
      if (cY < lines.size() - 1) {
        if (cY == firstShownLine + winRows - 2 - marginTop) {
          scrollDown();
        } else {
          cout << "\033[1B" << flush;
        }
        cY++;

        if (lines[cY].length() < cX) {
          printf("\033[%dD", cX - static_cast<int>(lines[cY].length()));
          cX = lines[cY].length();
        }
      }
      else {
        cX = lines.back().length();
        printf("\033[%d;%dH", cY - firstShownLine + marginTop, cX + marginLeft);
      }
    }

    void curLeft() {
      if (lines.empty()) {
        return;
      }
      if (cX > 0) {
        cout << "\033[1D" << flush;
        cX--;
      }
      else if (cX == 0 && curWrap && cY != 0) {
        if (cY == firstShownLine) {
          scrollUp();
        } else {
          printf("\033[1A");
        }
        cY--;
        cX = lines[cY].length();
        printf("\033[%dG", cX + marginLeft);
      }
    }

    void curRight() {      
      if (lines.empty()) {
        return;
      }
      if (cX < lines[cY].length()) {
        cout << "\033[1C" << flush;
        cX++;
      }
      else if (cX == lines[cY].length() && curWrap && cY < lines.size() - 1) {
        if (cY == firstShownLine + winRows - 2 - marginTop) {
          scrollDown();
        } else {
          printf("\033[1E");
        }
        cY++;
        cX = 0;
        printf("\033[%dG", marginLeft);
      }
    }

    void clsResetCur() {
      printf("\033[2J\033[%d;%dH", marginTop, marginLeft);
      fflush(stdout);
    }

    bool execCommand() {
      for(char c : comLineText) {
        if (c == ':') continue;
        if (find(validCommands.begin(), validCommands.end(), c) == validCommands.end()) {
          showErrorMsg("Not a valid command " + comLineText);
          return false;
        }
      }
      for(char ch : comLineText) {
        if (ch == ':') continue;
        else if (ch == 'w') overwriteFile();
        else if (ch == 'q') exitLep();
      }
      if (comLineText.length() > 1) return true;
      return false;
    }

    void showErrorMsg(string error) {
      comLineText = "\033[40;31m" + error + "\033[0m";
      updateCommandBar();
      sleep(1);
    }

    void getCurPosOnScr(int* x, int* y) {
      cout << "\033[6n";
      char response[32];
      int bytesRead = read(STDIN_FILENO, response, sizeof(response) - 1);
      response[bytesRead] = '\0';
      sscanf(response, "\033[%d;%dR", x, y);
    }

    void syncCurPosOnScr() {
      printf("\033[%d;%dH", cY - firstShownLine + marginTop, cX + marginLeft);
      fflush(stdout);
    }

    void scrollUp() {
      renderShownText(--firstShownLine);
    }

    void scrollDown() {
      renderShownText(++firstShownLine);
    }

    string alignR(string s, int w) {
      return string(w-s.length(), ' ') + s;
    }

    void checkLineNumSpace(string strLNum) {
      if (strLNum.length() > lineNumPad) {
        lineNumPad = strLNum.length();
        if (marginLeft - 1 <= lineNumPad) marginLeft++;
      }
    }

    string showLineNum(int lNum) {
      if (lineNum) {
        string strLNum = to_string(lNum);
        if (relativenumber) {
          if (lNum == 0) {
            checkLineNumSpace(to_string(cY+1));
            return "\033[0G\033[97m" + alignR(to_string(cY+1), lineNumPad) + "\033[0m"; // White highlight
          }
          return "\033[0G\033[90m" + alignR(strLNum, lineNumPad) + "\033[0m";
        }
        checkLineNumSpace(strLNum);
        if (lNum == cY + 1) {
          return "\033[0G\033[97m" + alignR(strLNum, lineNumPad) + "\033[0m"; // White highlight
        }
        return "\033[0G\033[90m" + alignR(strLNum, lineNumPad) + "\033[0m";
      }
      return "\0";
    }

    void updateLineNums(int startLine) {
      if (lineNum) {
        cout << "\033[s";
        printf("\033[%d;0H", marginTop + startLine - firstShownLine);
        if (relativenumber) {
          for(int i = startLine; i < lines.size() && i <= winRows-marginTop-2+startLine; i++) {
            cout << showLineNum(abs(cY-i)) << "\033[1E";
          }
        }
        else {
          for(int i = startLine; i < lines.size() && i <= winRows-marginTop-2+startLine; i++) {
            cout << showLineNum(i + 1) << "\033[1E";
          }
        }
        cout << "\033[u" << flush;
      }
    }

    void updateRenderedLines(int startLine, int count = -1) {
      cout << "\033[s";
      printf("\033[%d;0H", marginTop + startLine - firstShownLine);
      if (count < 0) count = lines.size();

      int maxIter = min(winRows - marginTop - 2 - (startLine - firstShownLine)
          , winRows - marginTop - 2 + 1);
      int maxIterWithOffset = min(maxIter, winRows - marginTop - 2 - (startLine - firstShownLine));

      for (int i = startLine; i < lines.size() && i <= startLine + maxIterWithOffset 
          && i - startLine < count; i++) {
        cout << "\033[2K";
        if (!relativenumber) cout << showLineNum(i + 1);
        printf("\033[%dG", marginLeft);
        cout << lines[i] << "\033[1E";
      }
      cout << "\033[u" << flush;
      if (relativenumber) updateLineNums(firstShownLine);
    }

    void renderShownText(int startLine) {
      cout << "\033[s";
      printf("\033[%d;1H", marginTop);
      for(int i = startLine; i < lines.size() && i <= winRows-marginTop-2+startLine; i++) {
        cout << "\033[2K";
        printf("\033[%dG", marginLeft);
        cout << lines[i] << "\033[1E";
      }
      if (lines.size() < winRows - marginTop - 2) {
        for (int i = 0; i <= winRows - marginTop - 2 - lines.size(); i++) {
          printf("\033[40;34m\033[2K\033[%dG~\033[1E", marginLeft);
        }
        cout << "\033[0m";
      }
      cout << "\033[u" << flush;
      updateLineNums(startLine);
    }

    void updateStatBar() {
      cout << "\033[s"; // Save cursor pos
      printf("\033[%d;0H\033[2K\r", winRows - 1);
      if (currentState != State::COMMAND) {
        printf("\033[%d;0H", winRows); // Move cursor to the last line
      }
      cout << "\033[2K\r"; // Clear current line
      cout << "\033[47;30m"; // Set bg color to white and text color to black
      string mode;
      if (currentState == State::INPUT) mode = " INPUT";
      else if (currentState == State::COMMAND) mode = " COMMAND";
      else if (currentState == State::VISUAL) mode = " VISUAL";
      else if (currentState == State::VLINE) mode = " V-LINE";
      else mode = " NORMAL";
      string saveText = " s ";
      string saveStatus;
      if (unsaved) saveStatus = "\033[41;30m" + saveText + "\033[47;30m";
      else saveStatus = "\033[42;30m" + saveText + "\033[47;30m";
      string eInfo = mode + " | " + fileName + " " + saveStatus;
      string curInfo = to_string(cY + 1) + ", " + to_string(cX + 1) + " ";
      cout << eInfo;
     
      int fillerSpace = winCols - eInfo.length() - curInfo.length() + (saveStatus.length() - saveText.length());
      cout << string(fillerSpace, ' ') << curInfo;
      // Restore text color and cursor pos
      cout << "\033[0m\033[u" << flush;
    }

    void updateCommandBar() {
      cout << "\033[s"; // Save cursor pos
      printf("\033[%d;1H", winRows); // Move cursor to the last line
      cout << "\033[2K\r"; // Clear current line
      cout << " " << comLineText;
      cout << "\033[0m\033[u" << flush;
    }

    void updateSelectedText() {
      if (currentState == State::VISUAL) {
        selectedText.eX = cX;
        selectedText.eY = cY;
      }
      else if (currentState == State::VLINE) {
        selectedText.eX = lines[cY].length();
        selectedText.eY = cY;
      }
    }

    void updateShowSelection() {
      if (selectedText.isNull()) return;
      textArea sel = selectedText;
      checkSelectionPoints(&sel);

      if (sel.bY < firstShownLine) {
        sel.bY = firstShownLine;
        sel.bX = 0;
      }
      if (sel.eY > firstShownLine + winRows - marginTop - 2) {
        sel.eY = firstShownLine + winRows - marginTop - 2;
        sel.eX = lines[sel.eY].length();
      }

      updateRenderedLines((sel.bY == firstShownLine) ? firstShownLine : sel.bY-1, sel.eY-sel.bY + 3);

      cout << "\033[s\033[7m"; // Inverse color
      printf("\033[%d;%dH", marginTop + sel.bY - firstShownLine, marginLeft + sel.bX);
      if (sel.bY == sel.eY) {
        cout << lines[sel.bY].substr(sel.bX, sel.eX-sel.bX + 1);
      } else {
        cout << lines[sel.bY].substr(sel.bX) << "\033[1E";
        for (int i = sel.bY + 1; i < sel.eY; i++) {
          printf("\033[%dG", marginLeft);
          cout << lines[i] << "\033[1E";
        }
        printf("\033[%dG", marginLeft);
        cout << lines[sel.eY].substr(0, sel.eX + 1);
      }
      cout << "\033[0m\033[u" << flush;
    }

    void copySelection() {
      if (selectedText.isNull()) return;
      checkSelectionPoints(&selectedText);
      clipboard.clear();

      int startX = selectedText.bX;
      int endX = selectedText.eX;
      for (int i = selectedText.bY; i <= selectedText.eY; i++) {
        int len = lines[i].length() - startX;
        if (i == selectedText.eY) {
          len = (endX == lines[i].length()) ? lines[i].length() - startX : endX - startX + 1;
        }
        clipboard.push_back(lines[i].substr(startX, len));
        startX = 0;
      }
    }

    void deleteSelection() {
      if (selectedText.isNull()) return;
      unsaved = true;
      checkSelectionPoints(&selectedText);
      
      int startLine = selectedText.bY;
      int endLine = selectedText.eY;
      
      if (startLine == endLine)  {
        if (lines[startLine].length() == selectedText.eX - selectedText.bX) {
          lines.erase(lines.begin() + startLine);
        } else {
          if (selectedText.eX == lines[startLine].length()) {
            lines[startLine].erase(selectedText.bX);
            lines[startLine].append(lines[startLine + 1]);
            lines.erase(lines.begin() + startLine + 1);
          } else {
            lines[startLine].erase(selectedText.bX, selectedText.eX - selectedText.bX + 1);
          }
        }
      } else {
        int delLines = 0;
        lines[startLine].erase(selectedText.bX);
        if (endLine - startLine > 1) {
          lines.erase(lines.begin() + startLine + 1, lines.begin() + endLine);
          delLines = endLine - startLine - 1;
        }

        if (selectedText.eX == lines[endLine - delLines].length()) {
          lines.erase(lines.begin() + endLine - delLines);
        } else {
          lines[endLine - delLines].erase(0, selectedText.eX + 1);
        }
        lines[startLine].append(lines[endLine - delLines]);
        lines.erase(lines.begin() + endLine - delLines);
      }

      if (lines[cY].empty()) cX = 0;
      else {
        cX = min(selectedText.bX, static_cast<int>(lines[startLine].length()));
      }
      cY = selectedText.bY;
      syncCurPosOnScr();
      selectedText.clear();
      updateRenderedLines(startLine);
    }
    
    void checkSelectionPoints(textArea* selection) {
      if (currentState == State::VISUAL) {
        if (selection->eY < selection->bY || 
            (selection->bY == selection->eY && selection->eX < selection->bX)) {
          swap(selection->bY, selection->eY);
          swap(selection->bX, selection->eX);
        }
      }
      else if (currentState == State::VLINE) {
        if (selection->eY < selection->bY) {
          swap(selection->bY, selection->eY);
          selection->bX = 0;
          selection->eX = lines[selection->eY].length();
        }
      }
    }

    void pasteClipboard() {
      if (clipboard.empty()) return;
      unsaved = true;
      int begY = cY;

      string remain = "";
      for (string line : clipboard) {
        if (cX == lines[cY].length()) {
          lines.insert(lines.begin() + cY + 1, line);
          cY++;
          cX = lines[cY].length();
        }
        else {
          remain = lines[cY].substr(cX + 1);
          lines[cY].erase(cX + 1);
          lines[cY].append(line);
          cX = lines[cY].length();
        }
      }
      lines[cY].append(remain);
      cX--;

      if (cY - firstShownLine > winRows - marginTop - 2) {
        firstShownLine = cY - winRows + marginTop + 2;
        renderShownText(firstShownLine);
      }
      else if (cY == begY) {
        updateRenderedLines(begY, 1);
      } else {
        updateRenderedLines(begY);
      }
      syncCurPosOnScr();
    }

    void getWinSize() {
      struct winsize size;
      if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) {
          cerr << "Failed to get terminal window size" << endl;
          return;
      }
      winRows = size.ws_row;
      winCols = size.ws_col;
    }

    void exitLep() {
      if (unsaved) {
        comLineText = "Unsaved changes. Save changes [y/n]:";
        updateStatBar();
        updateCommandBar();
        string ans = "";
        while(1) {
          char c = readKey();
          if (c == 10) {
            if (ans == "y") {
              overwriteFile();
              break;
            }
            else if (ans == "n") break;
          }
          else if (c == 127 && ans.length() > 0) {
            comModeDelChar();
            ans.pop_back();
            updateCommandBar();
          }
          else if (c == 27) {
            comLineText.clear();
            handleEvent(Event::BACK);
            return;
          }
          else if (!iscntrl(c) && c < 127) {
            ans += c;
            comLineText += c;
            updateCommandBar();
          }
        }
      }
      system("clear");
      exit(0);
    }
};

void dRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &defSet);
}

void eRawMode() {
  tcgetattr(STDIN_FILENO, &defSet);
  atexit(dRawMode);

  struct termios raw = defSet;
  raw.c_lflag &= ~(ECHO | ICANON);
  raw.c_cc[VTIME] = 1; //Timeout time in 1/10 of seconds
  raw.c_cc[VMIN] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char** argv) {
	system("clear");
	eRawMode();

  string fileName;
  if (argc >= 2) fileName = argv[1];
  else {
    fileName = "fileNameNotGiven.txt";
  }

  Lep lep;
  lep.readFile(fileName);

  while(1) {
    switch (lep.currentState) {
      case State::INPUT:
        lep.modeInput();
        break;

      case State::COMMAND:
        lep.modeCommand();
        break;

      case State::VLINE:
      case State::VISUAL:
        lep.modeVisual();
        break;
  
      default:
        lep.modeNormal();      
        break;
    }
  }

	return 0;
}
