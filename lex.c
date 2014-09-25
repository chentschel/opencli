%{

#define YY_DECL		int lexscan(char *buffer, char **_args)
#define MAX_ARGS	10

%}

WORD	[a-zA-Z0-9\/\.-]+
SPECIAL	[()><|&;*]

%%
	yy_scan_string(buffer);
	
	int _argcount = 0; 
	_args[0] = NULL; 

{WORD}|{SPECIAL} {
	if (_argcount < (MAX_ARGS - 1)) {
		_args[_argcount++] = (char *)strdup(yytext);
		_args[_argcount] = NULL;
	}
}

\r	|
<<EOF>>	{
	yy_delete_buffer(YY_CURRENT_BUFFER);
	return _argcount;
}

[ \t]+

.

%%

int getline(char *buff, char **args)
{
	return lexscan(buff, args);
}
