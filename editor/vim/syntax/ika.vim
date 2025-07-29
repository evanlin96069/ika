" ika.vim - Syntax highlighting for ika language

" Preprocessor directive
syntax match ikaPreproc /^\s*#\s*\(include\|define\|undef\|if\|elif\|else\|endif\)/

syntax match ikaErrorDirective /^\s*#\s*error\>/ nextgroup=ikaErrorMsg skipwhite
syntax match ikaErrorMsg /.*/ contained

syntax match ikaWarningDirective /^\s*#\s*warning\>/ nextgroup=ikaWarningMsg skipwhite
syntax match ikaWarningMsg /.*/ contained

" Keyword groups
syntax keyword ikaKeyword else if return while break continue asm
syntax keyword ikaDecl var const fn extern pub packed enum struct true false null
syntax keyword ikaBuiltinOp sizeof as
syntax keyword ikaLiteral true false null
syntax keyword ikaType void bool u8 u16 u32 i8 i16 i32

" Comment
syntax match ikaComment "//.*$"

" String and character literals
syntax region ikaString start=/"/ skip=/\\./ end=/"/ contains=ikaEscape
syntax region ikaChar start=/'/ skip=/\\./ end=/'/ contains=ikaEscape

" Escape sequences
syntax match ikaEscape /\\[nrt\\'\"0]/
syntax match ikaEscape /\\x\x\{2}/

" Numbers: binary, hex, decimal
syntax match ikaNumber /\<0[bB][01]\+\>/
syntax match ikaNumber /\<0[xX][0-9a-fA-F]\+\>/
syntax match ikaNumber /\<[0-9]\+\>/

" Highlight groups
highlight link ikaKeyword Keyword
highlight link ikaDecl Statement
highlight link ikaBuiltinOp Function
highlight link ikaLiteral Boolean
highlight link ikaType Type
highlight link ikaPreproc PreProc
highlight link ikaErrorDirective PreProc
highlight link ikaWarningDirective PreProc
highlight link ikaErrorMsg String
highlight link ikaWarningMsg String
highlight link ikaComment Comment
highlight link ikaString String
highlight link ikaChar Character
highlight link ikaEscape SpecialChar
highlight link ikaNumber Number
