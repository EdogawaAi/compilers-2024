%filenames = "scanner"

/* Please don't modify the lines above.*/

digit [0-9]
letter [a-zA-Z]
digits {digit}+
characters [0-9A-Za-z_]
quote \"
invisible [\000-\040]


/* You can add lex definitions here. */
/* TODO: Put your lab2 code here */

%x COMMENT STR IGNORE

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* reserved words */
 /*copilot saves my boring time*/
 /*https://github.com/kepingwang/tiger-compiler/blob/master/tiger.lex*/
"array" {adjust(); return Parser::ARRAY;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"break" {adjust(); return Parser::BREAK;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"do" {adjust(); return Parser::DO;}
"of" {adjust(); return Parser::OF;}
"nil" {adjust(); return Parser::NIL;}

 /* Operators */
  /* <INITIAL> "+" => ( Tokens.PLUS(yytext, yypos, yypos + (String.size yytext)) ); */

":=" {adjust(); return Parser::ASSIGN;}
"|" {adjust(); return Parser::OR;}
"&" {adjust(); return Parser::AND;}
">=" {adjust(); return Parser::GE;}
">" {adjust(); return Parser::GT;}
"<=" {adjust(); return Parser::LE;}
"<" {adjust(); return Parser::LT;}
"<>" {adjust(); return Parser::NEQ;}
"=" {adjust(); return Parser::EQ;}
"/" {adjust(); return Parser::DIVIDE;}
"*" {adjust(); return Parser::TIMES;}
"-" {adjust(); return Parser::MINUS;}
"+" {adjust(); return Parser::PLUS;}
"." {adjust(); return Parser::DOT;}
"}" {adjust(); return Parser::RBRACE;}
"{" {adjust(); return Parser::LBRACE;}
"]" {adjust(); return Parser::RBRACK;}
"[" {adjust(); return Parser::LBRACK;}
")" {adjust(); return Parser::RPAREN;}
"(" {adjust(); return Parser::LPAREN;}
";" {adjust(); return Parser::SEMICOLON;}
":" {adjust(); return Parser::COLON;}
"," {adjust(); return Parser::COMMA;}


 /*Identifier */
  /* <INITIAL> [a-zA-Z][0-9a-zA-Z_]* => ( Tokens.ID(yytext, yypos, yypos + (String.size yytext)) ); */
{letter}{characters}* {adjust(); return Parser::ID;}

 /* Integer */
  /* <INITIAL> [0-9]+ => ( Tokens.INT(yytext, yypos, yypos + (String.size yytext)) ); */
{digits} {adjust(); return Parser::INT;}



 /* string handling*/
{quote} {adjust(); begin(StartCondition_::STR);}

<STR>     {
    "\\n"       {adjustStr(); string_buf_ += '\n';}
    "\\t"       {adjustStr(); string_buf_ += '\t';}

    "\\"{digit}{digit}{digit} {
        adjustStr();
        std::string str = matched();
        string_buf_ += char((str[3] - '0') + (str[2] - '0') * 10 + (str[1] - '0') * 100);
    }

    "\\\""      {adjustStr(); string_buf_ += '\"';}
    "\\\\"      {adjustStr(); string_buf_ += '\\';}

    {quote}   {
      adjustStr();
      setMatched(string_buf_);
      string_buf_ = "";
      begin(StartCondition_::INITIAL); 
      return Parser::STRING;
    }
    .           {adjustStr(); string_buf_ += matched();}

    "\\"{invisible}       {
        adjustStr(); 
        ignore_buf_ += matched();
        begin(StartCondition_::IGNORE);
    }

    "\\^"[A-Z]        {adjustStr(); string_buf_ += char(matched()[2] - 'A' + 1);}

    \n {adjustStr(); string_buf_ += '\n';}
}

<IGNORE>  {
    "\\"              {adjustStr(); ignore_buf_ = ""; begin(StartCondition_::STR);}
    {invisible}       {adjustStr(); ignore_buf_ += matched();}
    .                 {adjustStr(); ignore_buf_ += matched(); string_buf_ += ignore_buf_; ignore_buf_ = ""; begin(StartCondition_::STR);}
}


  /* comment handling */
"/*" {adjust(); comment_level_++; begin(StartCondition_::COMMENT);}
<COMMENT>"/*" {adjust(); comment_level_++;}
<COMMENT>\n|. {adjust();}
<COMMENT>"*/" {
  adjust();
  comment_level_--;
  if (comment_level_ == 1) {
    begin(StartCondition_::INITIAL);
  }
}

 /* TODO: Put your lab2 code here */

 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
