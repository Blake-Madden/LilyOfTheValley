#include "import/docx_extract_text.h"
#include "import/hhc_hhk_extract_text.h"
#include "import/html_extract_text.h"
#include "import/odt_extract_text.h"
#include "import/postscript_extract_text.h"
#include "import/pptx_extract_text.h"
#include "import/unicode_extract_text.h"
//Be sure to include the *.cpp files from this library into your project too.
#include <string>
#include <iostream>

int main()
    {
    //TODO: insert your own HTML file here
    std::ifstream fs("PatientReport.htm", std::ios::in|std::ios::binary|std::ios::ate);
    if (fs.is_open())
        {
        //read an HTML file into a char* buffer first...
        size_t fileSize = fs.tellg();
        char* fileContents = new char[fileSize+1];
        std::auto_ptr<char> deleteBuffer(fileContents);
        std::memset(fileContents, 0, fileSize+1);
        fs.seekg(0, std::ios::beg);
        fs.read(fileContents, fileSize);

        //...then convert it to a Unicode buffer.
        //Note that you should use the specified character set
        //in the HTML file (usually UTF-8) when loading this file into a wchar_t buffer.
        //Here we are using the standard mbstowcs() function which assumes the current
        //system's character set; however, it is recommended to parse the character set
        //from the HTML file (with parse_charset()) and use a
        //system-dependent function (e.g., MultiByteToWideChar() on Win32)
        //to properly convert the char buffer to Unicode using that character set.
        wchar_t* convertedFileContents = new wchar_t[fileSize+1];
        std::auto_ptr<wchar_t> deleteConvertedBuffer(convertedFileContents);
        const size_t convertedFileSize = std::mbstowcs(convertedFileContents, fileContents, fileSize);

        //convert the Unicode HTML data into raw text
        lily_of_the_valley::html_extract_text htmlExtract;
        htmlExtract(convertedFileContents, convertedFileSize, true, false);
        //The raw text from the file is now in a Unicode buffer.
        //This buffer can be accessed from get_filtered_text() and its length
        //from get_filtered_text_length(). Call these to copy the text into
        //a wide string.
        std::wstring fileText(htmlExtract.get_filtered_text(), htmlExtract.get_filtered_text_length());

        std::wcout << fileText;
        }

    return 0;
    }
