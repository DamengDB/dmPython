# dmPython.pyi
# Stubs for dmPython (DM8) based on DM8_dmPython 使用手册.
from datetime import date, datetime, time
from typing import (
    Any,
    Dict,
    Generic,
    Iterator,
    List,
    Optional,
    Sequence,
    Tuple,
    Type,
    TypeVar,
    Union,
    overload,
)

try:
    from typing_extensions import NotRequired, TypedDict, Unpack
except Exception:  # pragma: no cover
    NotRequired = TypedDict = Unpack = Any  # type: ignore[misc,assignment]

# ---- DB-API module globals ----
apilevel: str          # "2.0"
threadsafety: int      # 1
paramstyle: str        # "qmark"

version: str
buildtime: str

SHUTDOWN_DEFAULT: str
SHUTDOWN_ABORT: str 
SHUTDOWN_IMMEDIATE: str 
SHUTDOWN_TRANSACTIONAL: str 
SHUTDOWN_NORMAL: str 

DEBUG_CLOSE: int
DEBUG_OPEN: int
DEBUG_SWITCH: int
DEBUG_SIMPLE: int

ISO_LEVEL_READ_DEFAULT: int
ISO_LEVEL_READ_UNCOMMITTED: int
ISO_LEVEL_READ_COMMITTED: int
ISO_LEVEL_REPEATABLE_READ: int
ISO_LEVEL_SERIALIZABLE: int

DSQL_MODE_READ_ONLY: int
DSQL_MODE_READ_WRITE: int

DSQL_AUTOCOMMIT_ON: int
DSQL_AUTOCOMMIT_OFF: int

PG_UTF8: int
PG_GBK: int
PG_BIG5: int
PG_ISO_8859_9: int
PG_EUC_JP: int
PG_EUC_KR: int
PG_KOI8R: int
PG_ISO_8859_1: int
PG_SQL_ASCII: int
PG_GB18030: int
PG_ISO_8859_11: int

LANGUAGE_CN: int
LANGUAGE_EN: int
LANGUAGE_CNT_HK: int

DSQL_TRUE: int
DSQL_FALSE: int

DSQL_RWSEPARATE_ON: int
DSQL_RWSEPARATE_OFF: int

DSQL_TRX_ACTIVE: int
DSQL_TRX_COMPLETE: int

DSQL_MPP_LOGIN_GLOBAL: int
DSQL_MPP_LOGIN_LOCAL: int

DSQL_CB_PRESERVE: int
DSQL_CB_CLOSE: int

# ---- Exceptions (PEP 249 family) ----
class Warning(Exception): ...
class Error(Exception): ...
class InterfaceError(Error): ...
class DatabaseError(Error): ...
class DataError(DatabaseError): ...
class OperationalError(DatabaseError): ...
class IntegrityError(DatabaseError): ...
class InternalError(DatabaseError): ...
class ProgrammingError(DatabaseError): ...
class NotSupportedError(DatabaseError): ...

# ---- Type constructors (PEP 249) ----
def Date(year: int, month: int, day: int) -> date: ...
def Time(hour: Optional[int], minute: Optional[int], second: Optional[int], microsecond: Optional[int], tzinfo: Optional[object]) -> time: ...
def Timestamp(year: int, month: int, day: int, hour: Optional[int], minute: Optional[int], second: Optional[int], microsecond: Optional[int], tzinfo: Optional[object]) -> datetime: ...
def DateFromTicks(ticks: float) -> date: ...
def TimeFromTicks(ticks: float) -> time: ...
def TimestampFromTicks(ticks: float) -> datetime: ...
def StringFromBytes(data: Union[bytes, bytearray, memoryview]) -> str: ...

# ---- DB/driver type objects (used in Cursor.description etc.) ----
class STRING: ...
class BINARY: ...
class NUMBER: ...
class DATETIME: ...
class ROWID: ...

# DM specific/common type objects mentioned in the manual
class BOOLEAN: ...
class BIGINT: ...
class REAL: ...
class DATE: ...
class TIMESTAMP: ...
class DECIMAL: ...
class INTERVAL: ...
class YEAR_MONTH_INTERVAL: ...
class TIME_WITH_TIMEZONE: ...

class LOB: ...
class BLOB: ...
class CLOB: ...

class BFILE: ...
class exBFILE: ...
class TIME: ...
class DOUBLE: ...
class FIXED_STRING: ...
class FIXED_BINARY: ...
class TIMESTAMP_WITH_TIMEZONE: ...
class CURSOR: ...

# ---- Cursor row typing ----
RowTuple = Tuple[Any, ...]
RowDict = Dict[str, Any]
RowT = TypeVar("RowT", RowTuple, RowDict)

# ---- connect kwargs typing (from the manual’s property list) ----
class _ConnectKwargs(TypedDict, total=False):
    user: NotRequired[str]
    password: NotRequired[str]

    # address forms
    dsn: NotRequired[str]          # "host:port"
    host: NotRequired[str]         # host/server only one should be used
    server: NotRequired[str]
    port: NotRequired[int]

    # options
    access_mode: NotRequired[Any]
    autoCommit: NotRequired[bool]
    connection_timeout: NotRequired[int]  # seconds
    login_timeout: NotRequired[int]       # seconds
    txn_isolation: NotRequired[Any]
    app_name: NotRequired[str]
    compress_msg: NotRequired[bool]
    use_stmt_pool: NotRequired[bool]
    ssl_path: NotRequired[str]
    ssl_pwd: NotRequired[str]            # “only set before connect; not readable”
    mpp_login: NotRequired[bool]
    ukey_name: NotRequired[str]          # only set before connect
    ukey_pin: NotRequired[str]           # only set before connect
    rwseparate: NotRequired[Any]
    rwseparate_percent: NotRequired[int]
    cursor_rollback_behavior: NotRequired[Any]
    lang_id: NotRequired[Any]
    local_code: NotRequired[Any]
    cursorclass: NotRequired[Type["Cursor[Any]"]]
    schema: NotRequired[str]
    shake_crypto: NotRequired[str]
    dmsvc_path: NotRequired[str]
    parse_type: NotRequired[str]

# ---- Cursor ----
class Cursor(Generic[RowT]):
    # context manager
    def __enter__(self) -> Cursor[RowT]: ...
    def __exit__(self, exc_type: Any, exc: Any, tb: Any) -> Optional[bool]: ...

    # lifecycle
    def close(self) -> None: ...

    # execution (manual allows positional params, tuple/list params, mapping + kwargs)
    def execute(self, sql: Optional[str], *parameters: Any, **kwargsParams: Any) -> Cursor[RowT]: ...
    def executemany(self, sql: Optional[str], sequence_of_params: Sequence[Sequence[Any]]) -> Cursor[RowT]: ...
    def executedirect(self, sql: str) -> Cursor[RowT]: ...
    def callproc(self, procname: str, *parameters: Any, **kwargsParams: Any) -> List: ...
    def callfunc(self, funcname: str, *parameters: Any, **kwargsParams: Any) -> List: ...

    # prepare API shown in manual
    def prepare(self, sql: str) -> Any: ...

    # fetch
    def fetchone(self) -> Optional[RowT]: ...
    def next(self) -> Optional[RowT]: ...
    def fetchmany(self, rows: Optional[int] = ...) -> List[RowT]: ...
    def fetchall(self) -> List[RowT]: ...
    def nextset(self) -> Optional[bool]: ...

    # bind helpers
    def setinputsizes(self, *sizes: Any) -> None: ...
    def setoutputsize(self, size: int, column: Optional[int] = ...) -> None: ...

    # var factory (manual documents extra params as “compat only”)
    def var(
        self,
        typ: Any,
        size: Optional[int] = ...,
        arraysize: Optional[int] = ...,
        inconverter: Any = ...,
        outconverter: Any = ...,
        typename: Any = ...,
        encoding_errors: Any = ...,
        bypass_decode: Any = ...,
    ) -> Any: ...

    # iteration
    def __iter__(self) -> Iterator[RowT]: ...
    def __next__(self) -> RowT: ...

    # ---- attributes ----
    arraysize: int                 # manual example shows default 50
    description: Optional[Sequence[Tuple[Any, ...]]]
    column_names: Sequence[str]
    rowcount: int
    rownumber: int
    output_stream: int  # 0/1
    bindarraysize: int
    statement: str
    with_rows: bool
    lastrowid: str
    execid: int
    connection: Any
    lazy_rowcount: bool

# cursorclass variants
class TupleCursor(Cursor[RowTuple]): ...
class DictCursor(Cursor[RowDict]): ...

# ---- Connection ----
class Connection:
    # dmPython.connect == dmPython.Connection / dmPython.Connect (manual says equal)
    def __init__(self, *args: Any, **kwargs: Any) -> None: ...

    def __enter__(self) -> Connection: ...
    def __exit__(self, exc_type: Any, exc: Any, tb: Any) -> Optional[bool]: ...

    def close(self) -> None: ...
    def disconnect(self) -> None: ...
    def commit(self) -> None: ...
    def rollback(self) -> None: ...

    @overload
    def cursor(self) -> TupleCursor: ...
    @overload
    def cursor(self, cursorclass: Type[DictCursor]) -> DictCursor: ...
    @overload
    def cursor(self, cursorclass: Type[TupleCursor]) -> TupleCursor: ...
    @overload
    def cursor(self, cursorclass: Type[Cursor[Any]]) -> Cursor[Any]: ...

    # extra methods shown by help()
    def debug(self, *args: Any, **kwargs: Any) -> Any: ...
    def explain(self, *args: Any, **kwargs: Any) -> Any: ...
    def ping(self, *args: Any, **kwargs: Any) -> Any: ...
    def shutdown(self, *args: Any, **kwargs: Any) -> Any: ...

    # frequently used descriptors/attrs (many are read/write depending on state)
    user: str
    server: str
    host: str
    port: int
    dsn: str
    autoCommit: bool
    current_schema: str

    # the rest: keep permissive to avoid false positives
    access_mode: Any
    app_name: Any
    async_enable: Any
    auto_ipd: Any
    compress_msg: Any
    connection_dead: Any
    connection_timeout: Any
    current_catalog: Any
    cursor_rollback_behavior: Any
    inst_name: Any
    lang_id: Any
    local_code: Any
    login_timeout: Any
    max_row_size: Any
    mpp_login: Any
    packet_size: Any
    rwseparate: Any
    rwseparate_percent: Any
    server_code: Any
    server_status: Any
    server_version: Any
    ssl_path: Any
    str_case_sensitive: Any
    trx_state: Any
    txn_isolation: Any
    use_stmt_pool: Any
    warning: Any

    # DSQL_ATTR_* constants exist on the object/class (keep as int)
    DSQL_ATTR_ACCESS_MODE: int
    DSQL_ATTR_APP_NAME: int
    DSQL_ATTR_ASYNC_ENABLE: int
    DSQL_ATTR_AUTOCOMMIT: int
    DSQL_ATTR_AUTO_IPD: int
    DSQL_ATTR_COMPRESS_MSG: int
    DSQL_ATTR_CONNECTION_DEAD: int
    DSQL_ATTR_CONNECTION_TIMEOUT: int
    DSQL_ATTR_CURRENT_CATALOG: int
    DSQL_ATTR_CURRENT_SCHEMA: int
    DSQL_ATTR_CURSOR_ROLLBACK_BEHAVIOR: int
    DSQL_ATTR_INSTANCE_NAME: int
    DSQL_ATTR_LANG_ID: int
    DSQL_ATTR_LOCAL_CODE: int
    DSQL_ATTR_LOGIN_PORT: int
    DSQL_ATTR_LOGIN_SERVER: int
    DSQL_ATTR_LOGIN_TIMEOUT: int
    DSQL_ATTR_LOGIN_USER: int
    DSQL_ATTR_MAX_ROW_SIZE: int
    DSQL_ATTR_MPP_LOGIN: int
    DSQL_ATTR_PACKET_SIZE: int
    DSQL_ATTR_RWSEPARATE: int
    DSQL_ATTR_RWSEPARATE_PERCENT: int
    DSQL_ATTR_SERVER_CODE: int
    DSQL_ATTR_SERVER_VERSION: int
    DSQL_ATTR_SSL_PATH: int
    DSQL_ATTR_STR_CASE_SENSITIVE: int
    DSQL_ATTR_TRX_STATE: int
    DSQL_ATTR_TXN_ISOLATION: int
    DSQL_ATTR_USE_STMT_POOL: int

# ---- connect / Connect overloads (from manual examples) ----

# 1) keyword properties
@overload
def connect(**kwargs: Unpack[_ConnectKwargs]) -> Connection: ...
@overload
def Connect(**kwargs: Unpack[_ConnectKwargs]) -> Connection: ...

# 2) positional short form: connect(user, password, "ip:port")
@overload
def connect(user: str, password: str, dsn: str) -> Connection: ...
@overload
def Connect(user: str, password: str, dsn: str) -> Connection: ...

# 3) single connection string: "user/password@ip:port/schema"
@overload
def connect(conn_str: str) -> Connection: ...
@overload
def Connect(conn_str: str) -> Connection: ...

# ---- misc APIs mentioned in manual ----
def objectvar(connection: Connection, name: str, pkgname: Optional[str] = ..., schema: Optional[str] = ...) -> Any: ...

