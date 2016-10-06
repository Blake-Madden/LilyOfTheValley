/**@addtogroup Importing
@brief Classes for importing and parsing text.
@date 2005-2016
@copyright Oleander Software, Ltd.
@author Oleander Software, Ltd.
@details This program is free software; you can redistribute it and/or modify
it under the terms of the BSD License.
* @{*/

#ifndef __EXTRACT_TEXT_H__
#define __EXTRACT_TEXT_H__

#include <exception>
#include "../indexing/string_util.h"

///Namespace for text extracting classes.
namespace lily_of_the_valley
    {
    /**@brief Base class for text extraction (from marked-up formats).
       Derived classes will usually implement operator() to parse a formatted
       buffer and then store the raw text in here.*/
    class extract_text
        {
    public:
        ///Default constructor.
        extract_text() : m_log_message_separator(L"\n"),
                         m_owns_buffer(false), m_text_buffer_size(0),
                         m_filtered_text_length(0), m_text_buffer(NULL) {}
        ///Destructor.
        virtual ~extract_text()
            {
            if (is_using_internal_buffer())
                { delete [] m_text_buffer; }
            m_text_buffer = NULL;
            }
        ///@returns The text that has been extracted from the formatted stream.
        const wchar_t* get_filtered_text() const
            { return m_text_buffer; }
        ///@returns The length of the parsed text.
        size_t get_filtered_text_length() const
            { return m_filtered_text_length; }
        /**Sets the writable buffer to the specified external buffer. This object will not own this buffer
           and will not delete it, caller must assume ownership of it.
           @note If subsequent calls to allocate_text_buffer() requires a larger buffer,
           then the object will stop using this buffer and switch to using an internal one.
           Call is_using_internal_buffer() to confirm which type of buffer is being used.
           @param buffer The external buffer to write filtered text to.
           @param length The size of the external buffer.*/
        void set_writable_buffer(wchar_t* buffer, const size_t length)
            {
            if (is_using_internal_buffer())
                { delete [] m_text_buffer; }
            m_text_buffer = buffer;
            m_text_buffer_size = length;
            m_owns_buffer = false;
            m_filtered_text_length = 0;
            std::wmemset(m_text_buffer, 0, m_text_buffer_size);
            }
        /**@returns Whether an internal buffer owned by this object is storing the filtered text.
           This will return false if an external buffer specified by the caller is being used or
           if a buffer hasn't been allocated yet.*/
        bool is_using_internal_buffer() const
            { return m_owns_buffer; }
        ///@returns A report of any issues with the last read block.
        const std::wstring& get_log() const
            { return m_log; }
        /**Sets the string used to separate the messages in the log report.
           By default, messages are separated by newlines, so call this to separate them
           by something like commas (if you are needing a single-line report).
           @param separator The separator character to use.*/
        void set_log_message_separator(const std::wstring& separator)
            { m_log_message_separator = separator; }
 #ifndef __UNITTEST
    protected:
#endif
        /**Allocates (or resizes) the buffer to hold the parsed text. This must be
           called before using add_character() or add_characters(). If the new size is smaller
           than the current size, then the size remains the same.
           @param text_length The new size of the buffer.
           @returns False if allocation fails, true otherwise.*/
        bool allocate_text_buffer(const size_t text_length)
            {
            if (m_text_buffer_size < text_length+1)
                {
                //reset filtered text from previous call
                if (is_using_internal_buffer())
                    { delete [] m_text_buffer; }
                //whether the buffer hasn't be set yet or caller set it to an external buffer,
                //we have ran out of room and need to create our own now and will own it.
                m_owns_buffer = true;
                try
                    { m_text_buffer = new wchar_t[text_length+1]; }
                catch (std::bad_alloc)
                    {
                    log_message(L"Unable to allocate memory for extracting text from file.");
                    m_text_buffer_size = m_filtered_text_length = 0;
                    m_text_buffer = NULL;
                    return false;
                    }
                m_text_buffer_size = text_length+1;
                }
            std::wmemset(m_text_buffer, 0, m_text_buffer_size);

            m_filtered_text_length = 0;
            return true;
            }
        /**Adds a character to the parsed buffer.
           @param character The character to add.*/
        void add_character(const wchar_t character)
            {
            assert(character !=0 && "NULL terminator passed to add_character()!");
            if (character != 0)
                { m_text_buffer[m_filtered_text_length++] = character; }
            }
        /**Adds a string to the parsed buffer.
           @param characters The string to add.
           @param length The length of the string to add.*/
        void add_characters(const wchar_t* characters, const size_t length)
            {
            if (length == 0 || !characters)
                { return ; }
            string_util::strncpy(m_text_buffer+m_filtered_text_length, characters, length);
            m_filtered_text_length += length;
            }
        /**@returns A writable copy of the text that has been extracted from the formatted stream.
           This should only be used under special circumstances where you need to directly write to the buffer;
           otherwise, you should use add_character() or add_characters() to normally copy text to this buffer.*/
        wchar_t* get_writable_buffer()
            { return m_text_buffer; }
        /**Trims any trailing whitespace from the end of the parsed text.*/
        void trim()
            {
            while (m_filtered_text_length > 0)
                {
                if (std::iswspace(m_text_buffer[m_filtered_text_length-1]))
                    { m_text_buffer[--m_filtered_text_length] = 0; }
                else
                    { break; }
                }
            }
        /**Sets the length of the parsed text.
        @note Any text added before this call
        will still be there, only the recorded length of the parsed text will be changed.
        Any subsequent calls to add_*() will overwrite any older text beyond this new length.
        This function should only be called when needing to overwrite previously parsed text.
        @param length The new starting point to added any new text.*/
        void set_filtered_text_length(const size_t length)
            {
            assert(length <= m_text_buffer_size && "Custom text length cannot be larger than the buffer.");
            m_filtered_text_length = length;
            }
        ///Empties the log of any previous parsing issues.
        void clear_log()
            { m_log.clear(); }
        /**Adds a message to the report logging system.
           @param message The message to log.*/
        void log_message(const std::wstring& message)
            {
            if (m_log.empty())
                { m_log.append(message); }//first message won't need a separator in front of it
            else
                { m_log.append(m_log_message_separator+message); }
            }
    private:
        std::wstring m_log;
        std::wstring m_log_message_separator;
        //data
        bool m_owns_buffer;
        size_t m_text_buffer_size;
        size_t m_filtered_text_length;
        wchar_t* m_text_buffer;
        //disable copy construction
        extract_text(const extract_text&) {}
        void operator=(const extract_text&) const {}
        };
    }

/** @}*/

#endif //__EXTRACT_TEXT_H__
