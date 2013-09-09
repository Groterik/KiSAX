/*
 * KiSAX --- Kindly SAX
 *
KiSAX License Agreement (MIT License)
--------------------------------------

Copyright (c) 2012 Maksim Klimov, maxy.klimov<at>gmail.com

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/


/**
 * @mainpage
 * One-header simple implementation of SAX parser
 *
 *
 * Implementation contains single base class KiSAX::Parser with clean and simple interface.
 * This class parses various valid and invalid XML files.
 *
 * Example of usage:
 * @code
    #include <iostream>
    #include <fstream>
    #include "kisax.h"

    using namespace std;

    // Simple example of printing XML structure to stdout
    class PrintXmlStructure: public KiSAX::Parser
    {
    private:
        virtual void documentStart()
        {
            cout<<"Document start\n";
        }
        virtual void documentEnd()
        {
            cout<<"Document end\n";
        }
        virtual void elementStart(const std::string &tagname, const stringmap &attributes)
        {
            cout<<"Tag start: "<<tagname<<'\n';
        }
        virtual void elementEnd(const std::string &tagname)
        {
            cout<<"Tag end: "<<tagname<<'\n';
            // stop parsing if tag "MyStopTag" is handled
            if (tagname == "MyStopTag") stop();
        }
        virtual void textHandle(const std::string &text)
        {
            cout<<"Raw text: "<<text<<'\n';
        }
    };

    int main()
    {
        std::ifstream inputFile("example.xml");
        if (!inputFile) return 1;
        PrintXmlStructure inst;
        inst.bind(inputFile);
        inst.parse();
        return 0;
    }
 * @endcode
 *
 * @author Maksim Klimov maxy.klimov<at>gmail.com
 *
 */
#ifndef KISAX_H_GUARD
#define KISAX_H_GUARD

#include <istream>
#include <string>
#include <map>
#include <exception>
#include <cctype>

namespace KiSAX
{

/**
 * @brief The Exception class
 *
 * This class implements exceptions that can be thrown by Parser
 *
 */
class Exception: public std::exception
{
public:

    /// Error codes
    enum ErrorCodes
    {
        KISAX_ERROR_CLOSE_STYLE_TAG = 200,
        KISAX_ERROR_CLOSING_TAG_WITH_NO_BODY,
        KISAX_ERROR_CLOSING_TAG_WITH_ATTRIBUTES,
        KISAX_ERROR_BAD_STREAM,
        KISAX_ERROR_PARSING
    };

    /**
     * @brief Exception constructor
     * @param n identification number of exception thrown
     */
    Exception(int n=0) throw():exception(),m_errno(n) { }

    virtual ~Exception() throw() {}
    /**
     * @brief Get error description
     * @return C-style character string describing the cause of current error
     */
    virtual const char *what() const throw()
    {
        switch(m_errno)
        {
        case KISAX_ERROR_CLOSE_STYLE_TAG:              return "Error: closing style-tag";
        case KISAX_ERROR_CLOSING_TAG_WITH_NO_BODY:     return "Error: closing tag without body";
        case KISAX_ERROR_CLOSING_TAG_WITH_ATTRIBUTES:  return "Error: closing tag with attributes";
        case KISAX_ERROR_BAD_STREAM:                   return "Error: bad input stream";
        case KISAX_ERROR_PARSING:                      return "Error: parsing";
        default: return "Unknown error";
        }
    }
private: /// @cond PRIVATE_STUFF

    /**
     * @brief Error identification number
     */
    int m_errno;

    /// @endcond
};



/**
 * @brief The main Parser class
 *
 * This base class implements Simple API for XML (SAX)
 *
 */
class Parser
{
public:

    /**
     * @brief Rules type that can be executed while parsing.
     */
    typedef void (Parser::*RULE)();

    /**
     * @brief String-map with key and values as string
     */
    typedef std::map< std::string, std::string > stringmap;

    /**
     * @brief Create an instance not bound to any stream.
     */
    Parser():m_state(0),m_is(0)
    {
        init();
    }


    /**
     * @brief Create an instance and bind it to the stream.
     */
    Parser(std::istream& istream):m_state(0),m_is(&istream)
    {
        init();
    }

    /**
     * @brief Bind parser to the stream
     *
     * Bind to input stream.
     * @return *this
     */
    Parser& bind(std::istream& istream)
    {
        m_is = &istream;
        return *this;
    }

    /**
     * @brief Parse document
     *
     * Start parsing document from the bound stream.
     */
    void parse()
    {
        if (!m_is) throw Exception(Exception::KISAX_ERROR_BAD_STREAM);
        if (!m_stop_flag) documentStart();
        m_stop_flag = false;
        while (m_is->get(m_current_char)&&!m_stop_flag)
        {
            m_state = m_states[m_state*MAXTOKEN+getSymbolType(m_current_char)];
            if (m_state<0) throw Exception(Exception::KISAX_ERROR_PARSING);
            (this->*(m_rules[m_state]))();
        }
        if (!m_stop_flag&&!m_is->eof()) throw Exception(Exception::KISAX_ERROR_BAD_STREAM);
        documentEnd();
    }

    /**
     * @brief Stop parsing
     *
     * This method can be called from handling methods in order to stop parsing.
     * The state of parser is stored (but stream can be changed) and the next call of parse() can finish its work.
     */
    void stop()
    {
        m_stop_flag = true;
    }

    /**
     * @brief Reset state of parser
     *
     * After this method is called, the state of parser is reset (but not stream).
     * So you can start parsing new document.
     */
    void reset()
    {
        m_state = 0;
        m_is_style       = false;
        m_is_closing_tag = false;
        m_is_tag_no_body = false;
        m_stop_flag      = false;
        do_all_clear();
    }

    /**
     * @brief Get access to bound stream
     * @return reference to stream
     */
    std::istream& getStream() { return *m_is; }

    virtual ~Parser(){}

private:

    /**
     * @brief This method is called when the document begins being parsed.
     *
     * Do nothing by default.
     */
    virtual void documentStart(){}

    /**
     * @brief This method is called when the document has already been completely parsed.
     *
     * Do nothing by default.
     */
    virtual void documentEnd(){}

    /**
     * @brief This method is called when new element appears in the document.
     *
     * The start-tags and the empty-element tags lead to opening of the new element.
     * So in these two cases this function is called.
     *
     * Do nothing by default.
     * @param[in] tagname
     * string containing the name of tag
     * @param[in] attributes
     * map of string-pairs ("attribute name","attribute value")
     *
     * For example, method overloaded in derived class:
     * @code
       void elementStart(const std::string &tagname, const stringmap &attributes)
       {
           std::cout<<"TAG NAME = "<<tagname<<'\n';
           for (stringmap::const_iterator iter=attributes.begin();iter!=attributes.end();++iter)
               std::cout<<'\t'<<(*iter).first<<' '<<(*iter).second<<'\n';
       }
       @endcode
     * This code prints tagname and all the pairs of attributes' names and values.
     */
    virtual void elementStart(const std::string& tagname,
                               const stringmap& attributes){}

    /**
     * @brief This method is called when the element is closed in the document.
     *
     * The end-tags and the empty-element tags lead to closing of the element.
     * So in these two cases this function is called.
     *
     * Do nothing by default.
     * @param[in] tagname string containing the name of the tag
     * @remarks The empty-element tag imposes calls of both handling methods, elementStart() and elementEnd(),
     * with the same tagname.
     * @warning Correctness of tags nesting is not checked!
     */
    virtual void elementEnd(const std::string& tagname){}

    /**
     * @brief This method is called to handle text outside any tags.
     *
     * Do nothing by default.
     * @param[in] rawtext
     * string containing the element (as raw text)
     * @remarks Text outside the root element is handled as well.
     */
    virtual void textHandle(const std::string& rawtext){}

    /**
     * @brief This method is called when definition tag appears in the document.
     *
     * Do nothing by default.
     * @param[in] definition
     * string containing the name of definition
     * @param[in] attributes
     * map of string-pairs ("attribute name","attribute value")
     *
     * For example, method overloaded in derived class:
     * @code
       void definition_handle(const std::string &tagname, const stringmap &attributes)
       {
           if (definition.compare("xml")==0)
               std::cout<<"XML VERSION = "<<attributes.at("version")<<'\n';
       }
       @endcode
     * This code prints version of xml presented in the
     * first required definition.
     * @warning Document must begin with the definition if you use parse().
     */
    virtual void definitionHandle(const std::string& definition,
                                   const stringmap& attributes){}

    /**
     * @brief This method is called when a comment appears in the document.
     *
     * Do nothing by default.
     * @param[in] comment string containing the comment
     */
    virtual void commentHandle(const std::string& comment){}

private: /// @cond PRIVATE_STUFF

    enum Symbol
    {
        TAG_BEGIN = 0,QUESTION,DELIMITER,EQUAL,QUOTE,SLASH,EXCLAMATION,
        MINUS,UNDERLINE,DIGIT,ALPHA,COLON,TAG_END,ANOTHER,
        MAXTOKEN //must be last element
    };

    Symbol getSymbolType(char c)
    {
        if (std::isalpha(c)) return ALPHA;
        if (std::isdigit(c)) return DIGIT;
        switch (c)
        {
        case ' ' :
        case '\n':
        case '\t': return DELIMITER;
        case '<' : return TAG_BEGIN;
        case '>' : return TAG_END;
        case '?' : return QUESTION;
        case '=' : return EQUAL;
        case '\"': return QUOTE;
        case '/' : return SLASH;
        case '!' : return EXCLAMATION;
        case '-' : return MINUS;
        case '_' : return UNDERLINE;
        case ':' : return COLON;
        default  : return ANOTHER;
        }
    }

    void do_nothing(){}
    void do_style_on(){ m_is_style = true; }
    void do_tag_pushback() { m_tag.push_back(m_current_char); }
    void do_commentary_pushback() { m_comment.push_back(m_current_char); }
    void do_commentary_end()
    {
        commentHandle(m_comment);
        m_comment.clear();
        m_state = 8;
    }
    void do_comment_minus_mark()
    {
        m_comment.push_back('-');
        m_state = 21;
    }

    void do_tag_clear() { m_tag.clear(); }
    void do_text_end()
    {
        textHandle(m_text);
        m_text.clear();
        m_state = 2;
    }

    void do_attribute_store()
    {
        m_attributes.insert(std::make_pair(m_attribute_first,m_attribute_second));
        m_attribute_first.clear();
        m_attribute_second.clear();
    }
    void do_all_clear()
    {
        m_is_closing_tag =   false;
        m_is_style       =   false;
        m_is_tag_no_body =   false;
        m_text.clear();
        m_tag.clear();
        m_attributes.clear();
        m_attribute_first.clear();
        m_attribute_second.clear();
    }
    void do_attribute_first_pushback() { m_attribute_first.push_back(m_current_char); }
    void do_attribute_second_pushback() { m_attribute_second.push_back(m_current_char); }
    void do_is_style_on(){ if (!m_is_style) throw Exception(Exception::KISAX_ERROR_CLOSE_STYLE_TAG); }
    void do_tag_end()
    {
        if (m_is_style)
        {
            m_is_style = false;
            definitionHandle(m_tag,m_attributes);
        } else if (m_is_closing_tag)
        {
            if (!m_attributes.empty()) throw Exception(Exception::KISAX_ERROR_CLOSING_TAG_WITH_ATTRIBUTES);
            elementEnd(m_tag);
        } else elementStart(m_tag,m_attributes);
        if (m_is_tag_no_body) elementEnd(m_tag);
        do_all_clear();
        m_state = 8;
    }

    void do_tag_wobody_close()
    {
        if (m_is_style) throw Exception(Exception::KISAX_ERROR_CLOSE_STYLE_TAG);
        if (m_is_closing_tag) throw Exception(Exception::KISAX_ERROR_CLOSING_TAG_WITH_NO_BODY);
        m_is_tag_no_body = true;
    }

    void do_tag_is_closing()
    {
        if (m_is_style) throw Exception(Exception::KISAX_ERROR_CLOSE_STYLE_TAG);
        m_is_closing_tag = true;
    }

    void do_text_pushback()
    {
        m_text.push_back(m_current_char);
    }

    void init()
    {
        static const int _states[] = {
           //0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13
             1,-1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0 --- initial state
            -1,17,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 1 --- first '<', '?' is waiting
            -1,17,-1,-1,-1, 9,18,-1, 3, 3, 3,-1,-1,-1, // 2 --- start of tag
            -1, 4, 5,-1,-1, 7,-1, 3, 3, 3, 3, 3, 6,-1, // 3 --- reading tag name
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 6,-1, // 4 --- definition of doc is being closed
            -1, 4,-1,-1,-1, 7,-1,-1,-1,-1,11,-1, 6,-1, // 5 --- tag name is ended, attributes or end of tag are waiting
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 6 --- fictive state, end of tag, jump to state 8
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 6,-1, // 7 --- end of tag w/o body
             2,10, 8,10,10,10,10,10,10,10,10,10,10,10, // 8 --- searching next tag or text body
            -1,-1, 0,-1,-1,-1,-1,-1, 3, 3, 3,-1,-1,-1, // 9 --- reading tag name (closing tag)
            16,10,10,10,10,10,10,10,10,10,10,10,10,10, //10 --- read text
            -1,-1,-1,12,-1,-1,-1,11,11,11,11,-1,-1,-1, //11 --- read attribute name
            -1,-1,-1,-1,14,-1,-1,-1,-1,-1,-1,-1,-1,-1, //12 --- attribute name is given, '=' is read, '"' is waiting
            13,13,13,13,15,13,13,13,13,13,13,13,13,13, //13 --- get attribute value
            13,13,13,13,15,13,13,13,13,13,13,13,13,13, //14 --- skip '"' and try to get attributes
            -1, 4, 5,-1,-1, 7,-1,-1,-1,-1,-1,-1, 6,-1, //15 --- store attributes, waiting new attributes or tag end
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //16 --- fictive state, event for text, clear it and jump to state 2
            -1,-1,-1,-1,-1,-1,-1,-1, 3, 3, 3,-1,-1,-1, //17 --- definition tag is opened
            -1,-1,-1,-1,-1,-1,-1,19,-1,-1,-1,-1,-1,-1, //18 --- "<!" situation, start of commentary (may be)
            -1,-1,-1,-1,-1,-1,-1,20,-1,-1,-1,-1,-1,-1, //19 --- "<!-" situation, start of commentary (may be)
            21,21,21,21,21,21,21,22,21,21,21,21,21,21, //20 --- "<!--", ready to start reading of commentary
            21,21,21,21,21,21,21,22,21,21,21,21,21,21, //21 --- read commentary
            24,24,24,24,24,24,24,23,24,24,24,24,24,24, //22 --- "<!--comment-", may be end of commentary
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,25,-1, //23 --- "<!--comment--", end of commentary
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //24 --- fictive state, minus in commentary, add it and jump to state 21
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //25 --- fictive state, rise commentary event and jump to state 8
        };
        static RULE _rules[] = {
            &Parser::do_nothing,                   // 0
            &Parser::do_nothing,                   // 1
            &Parser::do_nothing,                   // 2
            &Parser::do_tag_pushback,              // 3
            &Parser::do_is_style_on,               // 4
            &Parser::do_nothing,                   // 5
            &Parser::do_tag_end,                   // 6
            &Parser::do_tag_wobody_close,          // 7
            &Parser::do_nothing,                   // 8
            &Parser::do_tag_is_closing,            // 9
            &Parser::do_text_pushback,             //10
            &Parser::do_attribute_first_pushback,  //11
            &Parser::do_nothing,                   //12
            &Parser::do_attribute_second_pushback, //13
            &Parser::do_nothing,                   //14
            &Parser::do_attribute_store,           //15
            &Parser::do_text_end,                  //16
            &Parser::do_style_on,                  //17
            &Parser::do_nothing,                   //18
            &Parser::do_nothing,                   //19
            &Parser::do_nothing,                   //20
            &Parser::do_commentary_pushback,       //21
            &Parser::do_nothing,                   //22
            &Parser::do_nothing,                   //23
            &Parser::do_comment_minus_mark,        //24
            &Parser::do_commentary_end,            //25
        };
        m_rules = _rules;
        m_states = &(_states[0]);
        m_tag.reserve(127);
        m_text.reserve(10239);
        m_attribute_first.reserve(127);
        m_attribute_second.reserve(127);
        reset();
    }

    RULE *m_rules;

    int m_state;

    const int* m_states;

    char m_current_char;

    std::istream* m_is;
    std::string m_text;
    std::string m_tag;
    stringmap m_attributes;
    std::string m_attribute_first;
    std::string m_attribute_second;
    std::string m_comment;
    bool m_is_style;
    bool m_is_closing_tag;
    bool m_is_tag_no_body;
    bool m_stop_flag;

    /// @endcond
};

} // namespace KiSAX

#endif // KISAX_H
