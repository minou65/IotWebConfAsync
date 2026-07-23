/**
 * IotWebConfAsyncTab.h -- Extension of AsyncIotWebConf that adds tab support
 *   to the configuration page.
 *
 * Copyright (c) 2024 Andreas Zogg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _IOTWEBCONFASYNCTAB_h
#define _IOTWEBCONFASYNCTAB_h

#include "IotWebConfAsync.h"
#include <vector>
#include <map>

 /**
  * Structure to hold tab information
  */
struct AsyncTabInfo {
    const char* tabName;
    iotwebconf::ParameterGroup* group;
};

/**
 * Custom HTML format provider that adds tab support for async server
 */
class AsyncTabHtmlFormatProvider : public iotwebconf::HtmlFormatProvider {
public:
    AsyncTabHtmlFormatProvider(std::vector<AsyncTabInfo>* tabs, int minWidth = 500, int maxWidth = 600)
        : _tabs(tabs), _minWidth(minWidth), _maxWidth(maxWidth) {
    }

    /**
     * Set the container width constraints
     */
    void setContainerWidth(int minWidth, int maxWidth) {
        _minWidth = minWidth;
        _maxWidth = maxWidth;
    }

    String getStyleInner() override {
        String style = HtmlFormatProvider::getStyleInner();

        // Main container - configurable width for consistency across tabs
        style += "body > div{min-width:" + String(_minWidth) + "px;max-width:" + String(_maxWidth) + "px;width:100%;box-sizing:border-box;}\n";

        // Tab container styling
        style += ".tab{overflow:hidden;border-bottom:2px solid #16A1E7;background-color:#f1f1f1;margin-bottom:10px;display:flex;}\n";
        // Tab button styling
        style += ".tab button{background-color:#f1f1f1 !important;flex:1 1 0;min-width:0;border:1px solid #ccc !important;outline:none;cursor:pointer;";
        style += "padding:14px 16px;transition:0.3s;font-size:16px;border-top-left-radius:5px;border-top-right-radius:5px;";
        style += "margin-right:2px;border-bottom:none !important;color:#333 !important;line-height:normal !important;width:auto !important;box-sizing:border-box;}\n";
        // Tab button hover
        style += ".tab button:hover{background-color:#ddd !important;}\n";
        // Active tab button
        style += ".tab button.active{background-color:#16A1E7 !important;color:white !important;border:1px solid #16A1E7 !important;border-bottom:2px solid #fff !important;position:relative;z-index:1;}\n";
        // Tab content - ALL hidden by default
        style += ".tabcontent{display:none;padding:12px;border:1px solid #ccc;border-top:none;background-color:#fff;}\n";
        // Fieldsets consistent width
        style += "fieldset{width:100%;box-sizing:border-box;}\n";

        return style;
    }

private:
    std::vector<AsyncTabInfo>* _tabs;
    int _minWidth;
    int _maxWidth;
};

/**
 * Extended AsyncIotWebConf class with tab support
 */
class AsyncIotWebConfTab : public AsyncIotWebConf {
public:
    enum ChunkStepTab {
        CHUNK_TAB_HEAD = 0,
        CHUNK_TAB_SCRIPT,
        CHUNK_TAB_STYLE,
        CHUNK_TAB_HEADEXT,
        CHUNK_TAB_HEADEND,
        CHUNK_TAB_FORMSTART,
        CHUNK_TAB_TABSCRIPT,
        CHUNK_TAB_BUTTONS,
        CHUNK_TAB_SYSTEM_TAB_START,
        CHUNK_TAB_SYSTEMPARAMS,
        CHUNK_TAB_SYSTEM_CUSTOM,
        CHUNK_TAB_SYSTEM_TAB_END,
        CHUNK_TAB_CUSTOM_TABS_START,
        CHUNK_TAB_CUSTOM_TAB_START,
        CHUNK_TAB_CUSTOM_TAB_CONTENT,
        CHUNK_TAB_CUSTOM_TAB_END,
        CHUNK_TAB_FORMEND,
        CHUNK_TAB_UPDATE,
        CHUNK_TAB_CONFIGVER,
        CHUNK_TAB_END,
        CHUNK_TAB_DONE
    };

    AsyncIotWebConfTab(
        const char* defaultThingName, DNSServer* dnsServer, AsyncWebServerWrapper* webServerWrapper,
        const char* initialApPassword, const char* configVersion = "init")
        : AsyncIotWebConf(defaultThingName, dnsServer, webServerWrapper, initialApPassword, configVersion),
        _currentTabIndex(0),
        _currentTabGroupIndex(0),
        _currentTabChunkStep(CHUNK_TAB_HEAD),
        _systemTabName("System"),
        _systemCustomGroupIndex(0),
        _systemTabPosition(0) {  // Default: am Anfang
        _tabHtmlFormatProvider = new AsyncTabHtmlFormatProvider(&_tabs);
        setHtmlFormatProvider(_tabHtmlFormatProvider);
    }

    ~AsyncIotWebConfTab() {
        delete _tabHtmlFormatProvider;
    }

    void addParameterGroup(iotwebconf::ParameterGroup* group, const char* tabName) {
        AsyncIotWebConf::addParameterGroup(group);

        AsyncTabInfo tabInfo;
        tabInfo.tabName = tabName;
        tabInfo.group = group;
        _tabs.push_back(tabInfo);

        if (strcmp(tabName, _systemTabName) == 0) {
            return;
        }

        if (_uniqueTabNames.find(tabName) == _uniqueTabNames.end()) {
            _uniqueTabNames[tabName] = _uniqueTabsList.size();
            _uniqueTabsList.push_back(tabName);
        }
    }

    void addParameterGroup(iotwebconf::ParameterGroup* group) {
        addParameterGroup(group, "Configuration");
    }

    void setSystemTabName(const char* tabName) {
        _systemTabName = tabName;
    }

    /**
     * Set the position of the system tab
     * @param position Position index:
     *   0 = first position (default)
     *   1..n = specific position (1-based)
     *   -1 = last position
     */
    void setSystemTabPosition(int position) {
        _systemTabPosition = position;
    }

    std::vector<AsyncTabInfo>* getTabsVector() {
        return &_tabs;
    }

    size_t getNextChunk(uint8_t* buffer, size_t maxLen) override {
        bool dataArrived_ = false;
        size_t written_ = 0;
        const size_t MAX_INTERNAL_BUFFER = 32000;

        while (_currentTabChunkStep != CHUNK_TAB_DONE && written_ < maxLen) {
            yield();

            if (_chunkBufferPos >= _chunkBuffer.length()) {
                _chunkBuffer = "";
                _chunkBufferPos = 0;

                HtmlChunkCallback writer_ = [&](const char* data, size_t len) -> size_t {
                    yield();
                    size_t available = MAX_INTERNAL_BUFFER - _chunkBuffer.length();
                    if (available == 0) return 0;
                    size_t toWrite = (len < available) ? len : available;
                    size_t oldLen = _chunkBuffer.length();
                    _chunkBuffer.concat(data, toWrite);
                    return _chunkBuffer.length() - oldLen;
                    };

                switch (_currentTabChunkStep) {
                case CHUNK_TAB_HEAD:
                    _chunkBuffer = this->getHtmlFormatProvider()->getHead();
                    _chunkBuffer.replace("{v}", String("Config ") + this->getThingName());
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_SCRIPT:
                    _chunkBuffer = this->getHtmlFormatProvider()->getScript();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_STYLE:
                    _chunkBuffer = this->getHtmlFormatProvider()->getStyle();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_HEADEXT:
                    _chunkBuffer = this->getHtmlFormatProvider()->getHeadExtension();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_HEADEND:
                    _chunkBuffer = this->getHtmlFormatProvider()->getHeadEnd();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_FORMSTART:
                    _chunkBuffer = this->getHtmlFormatProvider()->getFormStart();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_TABSCRIPT:
                    _chunkBuffer = generateTabScript();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_BUTTONS:
                    _chunkBuffer = generateTabButtons();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_SYSTEM_TAB_START:
                    // System tab visibility depends on its position
                    _chunkBuffer = "<div id='" + String(_systemTabName) + "' class='tabcontent'";
                    if (_systemTabPosition == 0) {
                        _chunkBuffer += " style='display:block;'";
                    }
                    else {
                        _chunkBuffer += " style='display:none;'";
                    }
                    _chunkBuffer += ">\n";
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_SYSTEMPARAMS:
                    _lastStepFinished = this->getSystemParameterGroup()->renderHtml(dataArrived_, _webRequestWrapper, writer_);
                    break;
                case CHUNK_TAB_SYSTEM_CUSTOM:
                    if (_systemCustomGroupIndex < _tabs.size()) {
                        while (_systemCustomGroupIndex < _tabs.size() &&
                            strcmp(_tabs[_systemCustomGroupIndex].tabName, _systemTabName) != 0) {
                            _systemCustomGroupIndex++;
                        }

                        if (_systemCustomGroupIndex < _tabs.size() &&
                            strcmp(_tabs[_systemCustomGroupIndex].tabName, _systemTabName) == 0) {
                            _lastStepFinished = _tabs[_systemCustomGroupIndex].group->renderHtml(
                                dataArrived_, _webRequestWrapper, writer_);

                            if (_lastStepFinished) {
                                _systemCustomGroupIndex++;
                            }
                        }
                        else {
                            _lastStepFinished = true;
                        }
                    }
                    else {
                        _lastStepFinished = true;
                    }
                    break;
                case CHUNK_TAB_SYSTEM_TAB_END:
                    _chunkBuffer = "</div>\n";
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_CUSTOM_TABS_START:
                    _currentTabIndex = 0;
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_CUSTOM_TAB_START:
                    if (_currentTabIndex < _uniqueTabsList.size()) {
                        _chunkBuffer = "<div id='" + String(_uniqueTabsList[_currentTabIndex]) + "' class='tabcontent'";

                        // Determine if this tab should be visible on load
                        // Calculate the actual position of this custom tab
                        int actualPosition = _currentTabIndex;
                        if (_systemTabPosition >= 0 && _systemTabPosition <= (int)_currentTabIndex) {
                            actualPosition++; // System tab comes before this one
                        }

                        // First tab (position 0) should be visible
                        if (actualPosition == 0) {
                            _chunkBuffer += " style='display:block;'";
                        }
                        else {
                            _chunkBuffer += " style='display:none;'";
                        }
                        _chunkBuffer += ">\n";

                        _currentTabGroupIndex = 0;
                        _lastStepFinished = true;
                    }
                    else {
                        _currentTabChunkStep = static_cast<ChunkStepTab>(CHUNK_TAB_FORMEND - 1);
                        _lastStepFinished = true;
                    }
                    break;
                case CHUNK_TAB_CUSTOM_TAB_CONTENT:
                    if (_currentTabIndex < _uniqueTabsList.size()) {
                        const char* currentTab = _uniqueTabsList[_currentTabIndex];

                        while (_currentTabGroupIndex < _tabs.size() &&
                            strcmp(_tabs[_currentTabGroupIndex].tabName, currentTab) != 0) {
                            _currentTabGroupIndex++;
                        }

                        if (_currentTabGroupIndex < _tabs.size() &&
                            strcmp(_tabs[_currentTabGroupIndex].tabName, currentTab) == 0) {
                            _lastStepFinished = _tabs[_currentTabGroupIndex].group->renderHtml(
                                dataArrived_, _webRequestWrapper, writer_);

                            if (_lastStepFinished) {
                                _currentTabGroupIndex++;
                            }
                        }
                        else {
                            _lastStepFinished = true;
                        }
                    }
                    else {
                        _lastStepFinished = true;
                    }
                    break;
                case CHUNK_TAB_CUSTOM_TAB_END:
                    if (_currentTabIndex < _uniqueTabsList.size()) {
                        _chunkBuffer = "</div>\n";
                        _currentTabIndex++;
                        if (_currentTabIndex < _uniqueTabsList.size()) {
                            _currentTabChunkStep = static_cast<ChunkStepTab>(CHUNK_TAB_CUSTOM_TAB_START - 1);
                        }
                    }
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_FORMEND:
                    _chunkBuffer = this->getHtmlFormatProvider()->getFormEnd();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_UPDATE:
                    _chunkBuffer = this->getUpdateLinkHtml();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_CONFIGVER:
                    _chunkBuffer = this->getConfigVersionHtml();
                    _lastStepFinished = true;
                    break;
                case CHUNK_TAB_END:
                    _chunkBuffer = this->getHtmlFormatProvider()->getEnd();
                    _lastStepFinished = true;
                    break;
                default:
                    _chunkBuffer = "";
                    _lastStepFinished = true;
                    break;
                }

                _chunkBufferPos = 0;

                if (_chunkBuffer.length() == 0 && _lastStepFinished) {
                    _currentTabChunkStep = static_cast<ChunkStepTab>(_currentTabChunkStep + 1);
                    continue;
                }

                if (_chunkBuffer.length() == 0 && !_lastStepFinished) {
                    break;
                }
            }

            size_t toCopy_ = std::min(maxLen - written_, _chunkBuffer.length() - _chunkBufferPos);
            memcpy(buffer + written_, _chunkBuffer.c_str() + _chunkBufferPos, toCopy_);
            _chunkBufferPos += toCopy_;
            written_ += toCopy_;

            if (_chunkBufferPos >= _chunkBuffer.length()) {
                if (_lastStepFinished && _currentTabChunkStep != CHUNK_TAB_CUSTOM_TAB_CONTENT && _currentTabChunkStep != CHUNK_TAB_SYSTEM_CUSTOM) {
                    _currentTabChunkStep = static_cast<ChunkStepTab>(_currentTabChunkStep + 1);
                }
                break;
            }
            else {
                break;
            }
        }

        if (_currentTabChunkStep == CHUNK_TAB_DONE) {
            resetChunkState();
            return 0;
        }

        return written_;
    }

    void resetChunkState() override {
        AsyncIotWebConf::resetChunkState();
        _currentTabChunkStep = CHUNK_TAB_HEAD;
        _currentTabIndex = 0;
        _currentTabGroupIndex = 0;
        _systemCustomGroupIndex = 0;
    }

private:
    std::vector<AsyncTabInfo> _tabs;
    std::map<String, size_t> _uniqueTabNames;
    std::vector<const char*> _uniqueTabsList;
    AsyncTabHtmlFormatProvider* _tabHtmlFormatProvider = nullptr;
    const char* _systemTabName;
    int _systemTabPosition;  // NEW: Position des System-Tabs

    size_t _currentTabIndex;
    size_t _currentTabGroupIndex;
    size_t _systemCustomGroupIndex;
    ChunkStepTab _currentTabChunkStep;

    String generateTabScript() {
        String script = "<script type='text/javascript'>\n";
        script += "function openTab(evt,tabName){\n";
        script += "var i,tabcontent,tablinks;\n";
        script += "tabcontent=document.getElementsByClassName('tabcontent');\n";
        script += "for(i=0;i<tabcontent.length;i++){\n";
        script += "tabcontent[i].style.display='none';\n";
        script += "}\n";
        script += "tablinks=document.getElementsByClassName('tablinks');\n";
        script += "for(i=0;i<tablinks.length;i++){\n";
        script += "tablinks[i].className=tablinks[i].className.replace(' active','');\n";
        script += "}\n";
        script += "var tabElement=document.getElementById(tabName);\n";
        script += "if(tabElement){\n";
        script += "tabElement.style.display='block';\n";
        script += "}\n";
        script += "if(evt&&evt.currentTarget){\n";
        script += "evt.currentTarget.className+=' active';\n";
        script += "}\n";
        script += "}\n";
        script += "</script>\n";
        return script;
    }

    String generateTabButtons() {
        String buttons;
        buttons.reserve(512);  // Reserve memory upfront
        buttons = "<div class='tab'>\n";

        // Calculate system tab position
        int systemPos = _systemTabPosition;
        size_t totalCustomTabs = _uniqueTabsList.size();

        // Clamp position to valid range
        if (systemPos < 0) {
            systemPos = totalCustomTabs;  // End
        }
        if (systemPos > (int)totalCustomTabs) {
            systemPos = totalCustomTabs;
        }

        int customTabsAdded = 0;
        bool systemTabAdded = false;

        // Generate all tabs in order
        for (int pos = 0; pos <= (int)totalCustomTabs; pos++) {
            // Check if system tab should be at this position
            if (pos == systemPos && !systemTabAdded) {
                buttons += "<button type='button' class='tablinks";
                if (pos == 0) buttons += " active";  // First tab is active
                buttons += "' onclick='openTab(event,\"";
                buttons += _systemTabName;
                buttons += "\");'>";
                buttons += _systemTabName;
                buttons += "</button>\n";
                systemTabAdded = true;
            }

            // Add custom tab if available and we haven't added all yet
            if (customTabsAdded < (int)totalCustomTabs) {
                buttons += "<button type='button' class='tablinks";
                if (pos == 0 && !systemTabAdded) buttons += " active";
                buttons += "' onclick='openTab(event,\"";
                buttons += _uniqueTabsList[customTabsAdded];
                buttons += "\");'>";
                buttons += _uniqueTabsList[customTabsAdded];
                buttons += "</button>\n";
                customTabsAdded++;
            }
        }

        buttons += "</div>\n";
        return buttons;
    }

    friend class AsyncTabHtmlFormatProvider;
};
#endif