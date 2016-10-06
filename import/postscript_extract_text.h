/**@addtogroup Importing
@brief Classes for importing and parsing text.
@date 2005-2016
@copyright Oleander Software, Ltd.
@author Oleander Software, Ltd.
@details This program is free software; you can redistribute it and/or modify
it under the terms of the BSD License.
* @{*/

#ifndef __POSTSCRIPT_EXTRACT_TEXT_H__
#define __POSTSCRIPT_EXTRACT_TEXT_H__

#include "extract_text.h"

namespace lily_of_the_valley
    {
    /**@brief Class to extract text from a <b>Postscript</b> stream.
    @par Example:
    @code
    std::ifstream fs("C:\\users\\Mistletoe\\CheckupReport.ps", std::ios::in|std::ios::binary|std::ios::ate);
    if (fs.is_open())
        {
        //read a Postscript file into a char* buffer
        size_t fileSize = fs.tellg();
        char* fileContents = new char[fileSize+1];
        std::auto_ptr<char> deleteBuffer(fileContents);
        std::memset(fileContents, 0, fileSize+1);
        fs.seekg(0, std::ios::beg);
        fs.read(fileContents, fileSize);
        //convert the Postscript data into raw text
        lily_of_the_valley::postscript_extract_text psExtract;
        psExtract(fileContents, fileSize);
        //The raw text from the file is now in a Unicode buffer.
        //This buffer can be accessed from get_filtered_text() and its length
        //from get_filtered_text_length(). Call these to copy the text into
        //a wide string.
        std::wstring fileText(psExtract.get_filtered_text(), psExtract.get_filtered_text_length());
        }
    @endcode*/
    class postscript_extract_text : public extract_text
        {
    public:
        /**Main interface for extracting plain text from a <b>Postscript</b> buffer. Supports <b>Postscript</b> up to version 2.
           @param ps_buffer The Postscript text to convert to plain text.
           @param text_length The length of the Postscript buffer.
           @returns A pointer to the parsed text, or NULL upon failure.
            Call get_filtered_text_length() to get the length of the parsed text.
           @throws postscript_header_not_found If an invalid document.
           @throws postscript_version_not_supported if document is a newer version of Postscript that is not supported.*/
        const wchar_t* operator()(const char* ps_buffer, const size_t text_length);
        ///Exception thrown when a <b>Postscript</b> is missing its header (more than likely an invalid <b>Postscript</b> file).
        class postscript_header_not_found : public std::exception {};
        ///Exception thrown when an unsupport version of <b>Postscript</b> is being parsed.
        class postscript_version_not_supported : public std::exception {};
        };
    }

/** @}*/

#endif //__POSTSCRIPT_EXTRACT_TEXT_H__
