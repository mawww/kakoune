# https://www.w3schools.com/sql/default.asp
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*/?(?i)sql %{
    set-option buffer filetype sql
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=sql %{
    require-module sql
    set-option window static_words %opt{sql_static_words}
}

hook -group sql-highlight global WinSetOption filetype=sql %{
    add-highlighter window/sql ref sql
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/sql }
}


provide-module sql %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/sql regions
add-highlighter shared/sql/code default-region group
add-highlighter shared/sql/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/sql/single_string region "'" (?<!\\)(\\\\)*' fill string
add-highlighter shared/sql/comment1 region '--' '$' fill comment
add-highlighter shared/sql/comment2 region '#' '$' fill comment
add-highlighter shared/sql/comment3 region '/\*' '\*/' fill comment

evaluate-commands %sh{
    # Keywords
    keywords="ALTER|AS|ASC|AUTO_INCREMENT|CHECK|CONSTRAINT|CREATE|DATABASE|DEFAULT|DELETE|DESC|DISTINCT|DROP"
    keywords="${keywords}|EXISTS|FOREIGN KEY|FROM|FULL JOIN|FULL OUTER JOIN|GROUP BY|HAVING|INDEX|INNER JOIN"
    keywords="${keywords}|INSERT INTO|INTO|JOIN|LEFT JOIN|LEFT OUTER JOIN|LIMIT|MODIFY|NOT NULL|ON|ORDER BY|PRIMARY KEY"
    keywords="${keywords}|REFERENCES|RIGHT JOIN|RIGHT OUTER JOIN|SELECT|SELECT TOP|SET|TABLE|TRUNCATE|UNION|UNIQUE"
    keywords="${keywords}|UPDATE|VALUES|VIEW|WHERE"

    # Operators
    operators="ALL|AND|ANY|BETWEEN|EXISTS|IN|IS|LIKE|NOT|OR|SOME"

    # MySQL functions
    functions="ABS|ACOS|ADDDATE|ADDTIME|ASCII|ASIN|ATAN|AVG|BIN|BINARY|CASE|CAST|CEIL|CEILING"
    functions="${functions}|CHARACTER_LENGTH|CHAR_LENGTH|COALESCE|CONCAT|CONCAT_WS|CONNECTION_ID|CONV|CONVERT"
    functions="${functions}|COS|COT|COUNT|CURDATE|CURRENT_DATE|CURRENT_TIME|CURRENT_TIMESTAMP|CURRENT_USER"
    functions="${functions}|CURTIME|DATABASE|DATE|DATE_ADD|DATEDIFF|DATE_FORMAT|DATE_SUB|DAY|DAYNAME"
    functions="${functions}|DAYOFMONTH|DAYOFWEEK|DAYOFYEAR|DEGREES|DIV|EXP|EXTRACT|FIELD|FIND_IN_SET|FLOOR"
    functions="${functions}|FORMAT|FROM_DAYS|GREATEST|HOUR|IF|IFNULL|INSERT|INSTR|ISNULL|LAST_DAY"
    functions="${functions}|LAST_INSERT_ID|LCASE|LEAST|LEFT|LENGTH|LN|LOCALTIME|LOCALTIMESTAMP|LOCATE|LOG"
    functions="${functions}|LOWER|LPAD|LTRIM|MAKEDATE|MAKETIME|MAX|MICROSECOND|MID|MIN|MINUTE|MOD|MONTH"
    functions="${functions}|MONTHNAME|NOW|NULLIF|PERIOD_ADD|PERIOD_DIFF|PI|POSITION|POW|POWER|QUARTER|RADIANS"
    functions="${functions}|RAND|REPEAT|REPLACE|REVERSE|RIGHT|ROUND|RPAD|RTRIM|SECOND|SEC_TO_TIME|SESSION_USER"
    functions="${functions}|SIGN|SIN|SPACE|SQRT|STRCMP|STR_TO_DATE|SUBDATE|SUBSTR|SUBSTRING|SUBSTRING_INDEX"
    functions="${functions}|SUBTIME|SUM|SYSDATE|SYSTEM_USER|TAN|TIME|TIMEDIFF|TIME_FORMAT|TIMESTAMP"
    functions="${functions}|TIME_TO_SEC|TO_DAYS|TRIM|TRUNCATE|UCASE|UPPER|USER|VERSION|WEEK|WEEKDAY|WEEKOFYEAR"
    functions="${functions}|YEAR|YEARWEEK"

    # SQL Server functions
    functions="${functions}|CHAR|CHARINDEX|DATALENGTH|DATEADD|DATENAME|DATEPART|GETDATE|GETUTCDATE|ISDATE"
    functions="${functions}|ISNUMERIC|LEN|NCHAR|PATINDEX|SESSIONPROPERTY|STR|STUFF|USER_NAME"

    # MS Access functions
    functions="${functions}|Abs|Asc|Atn|Avg|Chr|Cos|Count|CurDir|CurrentUser|Date|DateAdd|DateDiff|DatePart"
    functions="${functions}|DateSerial|DateValue|Day|Environ|Exp|Fix|Format|Hour|InStr|InstrRev|Int|IsDate"
    functions="${functions}|IsNull|IsNumeric|LCase|Left|Len|LTrim|Max|Mid|Min|Minute|Month|MonthName|Now"
    functions="${functions}|Randomize|Replace|Right|Rnd|Round|RTrim|Second|Sgn|Space|Split|Sqr|Str|StrComp"
    functions="${functions}|StrConv|StrReverse|Sum|Time|TimeSerial|TimeValue|Trim|UCase|Val|Weekday"
    functions="${functions}|WeekdayName|Year"

    # Oracle functions
    functions="${functions}|ADD_MONTHS|ASCIISTR|BITAND|CHR|COMPOSE|COSH|DBTIMEZONE|DECOMPOSE|DUMP|INITCAP|INSTRB"
    functions="${functions}|INSTRC|LENGTHB|LENGTHC|MEDIAN|MONTHS_BETWEEN|NCHR|NEW_TIME|NEXT_DAY|REGEXP_COUNT"
    functions="${functions}|REGEXP_INSTR|REGEXP_REPLACE|REGEXP_SUBSTR|REMAINDER|ROWNUM|SESSIONTIMEZONE|SOUNDEX"
    functions="${functions}|SYSTIMESTAMP|TANH|TRANSLATE|TRUNC|TZ_OFFSET|VSIZE"

    # MySQL data types
    data_types="LONGBLOB|LONGTEXT|MEDIUMBLOB|MEDIUMTEXT|SET|TEXT|TINYTEXT"
    data_types_fn="BIGINT|BLOB|CHAR|DATE|DATETIME|DECIMAL|DOUBLE|ENUM|FLOAT|INT"
    data_types_fn="${data_types_fn}|MEDIUMINT|SMALLINT|TIME|TIMESTAMP|TINYINT|VARCHAR|YEAR"

    # SQL Server data types
    data_types="${data_types}|bigint|bit|cursor|date|datetime|datetime2|datetimeoffset|image|int|money|nchar|ntext"
    data_types="${data_types}|nvarchar|real|smalldatetime|smallint|smallmoney|sql_variant|table|text|time"
    data_types="${data_types}|timestamp|tinyint|uniqueidentifier|varbinary|xml"
    data_types_fn="${data_types_fn}|binary|char|decimal|float|numeric|nvarchar|varbinary|varchar|varchar"

    # MS Access data types
    data_types="${data_types}|Text|Memo|Byte|Integer|Long|Single|Double|Currency|AutoNumber|Date"
    data_types="${data_types}|Time|Ole Object|Hyperlink|Lookup Wizard"

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list sql_static_words ${keywords} ${operators} ${functions} ${data_types} ${data_types_fn} NULL" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/sql/code/ regex '(?i)\b(${functions})\(.*?\)' 0:function
        add-highlighter shared/sql/code/ regex '(?i)\b(${data_types_fn})\(.*?\)' 0:type
        add-highlighter shared/sql/code/ regex '(?i)\b(${keywords})\b' 0:keyword
        add-highlighter shared/sql/code/ regex '(?i)\b(${operators})\b' 0:operator
        add-highlighter shared/sql/code/ regex '(?i)\b(${data_types})\b' 0:type
    "
}

add-highlighter shared/sql/code/ regex '\+|-|\*|/|%|&|\||^|=|>|<|>=|<=|<>|\+=|-=|\*=|/=|%=|&=|^-=|\|\*=' 0:operator
add-highlighter shared/sql/code/ regex \bNULL\b 0:value
add-highlighter shared/sql/code/ regex \b\d+(?:\.\d+)?\b 0:value

}
