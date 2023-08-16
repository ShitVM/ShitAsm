# ShitVM 표준 라이브러리
- 현재 ShitAsm에서 표준 라이브러리를 `import` 하려면 작업 디렉터리(Working directory)에 [std](../std) 디렉터리가 존재해야 합니다.
- ShitVM 표준 라이브러리는 ShitVM 이외의 ShitBC 구현체에서는 제공되지 않을 수도 있습니다.

## 목차
- [참조 방법](#참조-방법)
- [`array` 모듈](#array-모듈)
	- [`getLength` 함수](#getlength-함수)
	- [`copy` 프로시저](#copy-프로시저)
- [`io` 모듈](#io-모듈)
	- [`Stream` 구조체](#stream-구조체)
	- [`getStdin`, `getStdout` 함수](#getstdin-getstdout-함수)
	- [`openReadonlyFile`, `openWriteonlyFile` 함수](#openreadonlyfile-openwriteonlyfile-함수)
	- [`closeFile` 프로시저](#closefile-프로시저)
	- [`readInt`, `readSignedInt` 함수](#readint-readsignedint-함수)
	- [`writeInt`, `writeSignedInt` 프로시저](#writeint-writesignedint-프로시저)
	- [`readLong`, `readSignedLong` 함수](#readint-readsignedint-함수)
	- [`writeLong`, `writeSignedLong` 프로시저](#writeint-writesignedint-프로시저)
	- [`readDouble` 함수](#readdouble-함수)
	- [`writeDouble` 프로시저](#writedouble-프로시저)
	- [`readChar32` 함수](#readchar32-함수)
	- [`writeChar32` 프로시저](#writechar32-프로시저)
	- [`readString32` 함수](#readstring32-함수)
	- [`writeString32` 프로시저](#writestring32-프로시저)
- [`string` 모듈](#string-모듈)
	- [`String32` 구조체](#string32-구조체)
	- [`create2` 함수](#create32-함수)
	- [`push` 프로시저](#push-프로시저)
	- [`concat` 프로시저](#concat-프로시저)
	- [`destroy` 프로시저](#destroy-프로시저)

## 참조 방법
표준 라이브러리는 여러 개의 모듈로 구성되어 있습니다.
- `array`
- `io`
- `string`

```
import "/std/array.sba" as io
```
예를 들어 `array` 모듈을 참조하고 싶다면, 위와 같은 `import` 구문을 작성하면 됩니다. 표준 라이브러리는 외부 라이브러리로 분류되기 때문에 모듈 경로가 절대 경로여야 합니다. 상대 경로인 경우, 즉 `std/array.sba`인 경우에는 의미가 완전히 달라지므로 주의하세요.

## `array` 모듈
이 모듈은 배열을 다룰 때 도움이 되는 함수를 제공합니다.

### `getLength` 함수
#### 원형
```
func getLength(arrayPtr)
```

#### 설명
이 함수는 배열의 길이를 구할 때 사용합니다. 배열에 대한 포인터(`arrayPtr`)를 받아 해당 배열의 길이를 반환합니다.

`arrayPtr`은 반드시 `pointer` 자료형이어야 합니다.

함수의 반환값은 `long` 자료형입니다.

#### 예제
```
push 10
apush int[]
store myArray

lea myArray
call getLength ; 10을 반환합니다.
```

### `copy` 프로시저
#### 원형
```
proc copy(destPtr, destBegin, srcPtr, srcBegin, count)
```

#### 설명
이 프로시저는 배열의 원소 여러 개를 다른 배열로 복사할 때 사용합니다. 목적지 배열에 대한 포인터(`destPtr`), 복사할 인덱스(`destBegin`), 출발지 배열에 대한 포인터(`srcPtr`), 복사될 인덱스(`srcBegin`), 복사할 원소 개수(`count`)를 받습니다.

`destPtr`, `srcPtr`은 반드시 `pointer` 자료형이어야 합니다. 두 배열은 반드시 원소의 자료형이 같아야 합니다.

`destBegin`, `srcBegin`, `count`는 반드시 `long` 자료형이어야 합니다.

#### 예제
```
; destArray = int[5]{ 0, 1, 2, 3, 4 }
; srcArray = int[5]{ 5, 6, 7, 8, 9 }

push 2l
push 0l
lea srcArray
push 3l
lea destArray
call copy ; destArray = int[5]{ 0, 1, 2, 5, 6 }이 됩니다.
```

## `io` 모듈
이 모듈은 입출력과 관련된 구조체와 함수를 제공합니다.

### `Stream` 구조체
#### 원형
```
struct Stream:
	long _handle
```

#### 설명
이 구조체는 스트림을 나타냅니다.

`_handle`의 값을 임의로 조작하지 마세요.

### `getStdin`, `getStdout` 함수
#### 원형
```
func getStdin
func getStdout
```

#### 설명
각각 표준 입력, 표준 출력에 대한 스트림을 가져올 때 사용합니다.

함수의 반환값은 `Stream` 구조체입니다.

### `openReadonlyFile`, `openWriteonlyFile` 함수
#### 원형
```
func openReadonlyFile(pathPtr)
func openWriteonlyFile(pathPtr)
```

#### 설명
각각 읽기 전용 파일 스트림, 쓰기 전용 파일 스트림을 생성할 때 사용합니다. 파일 경로에 대한 포인터(`pathPtr`)를 받아 스트림을 반환합니다.

`pathPtr`은 반드시 `pointer` 자료형이어야 합니다. `pathPtr`이 가리키는 곳의 자료형은 반드시 `string.String32` 구조체여야 합니다.

함수의 반환값은 `Stream` 구조체입니다.

스트림 사용을 마친 후 반드시 `closeFile` 프로시저를 사용해 스트림을 파괴해야 합니다. 그렇지 않으면 메모리 누수가 발생할 수 있습니다.

### `closeFile` 프로시저
#### 원형
```
proc closeFile(stream)
```

#### 설명
이 프로시저는 파일 스트림을 파괴할 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

### `readInt`, `readSignedInt` 함수
#### 원형
```
func readInt(stream)
func readSignedInt(stream)
```

#### 설명
각각 스트림으로부터 부호 없는 4바이트 정수, 부호 있는 4바이트 정수를 입력 받을 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

함수의 반환값은 `int` 자료형입니다.

### `writeInt`, `writeSignedInt` 프로시저
#### 원형
```
proc writeInt(stream, value)
proc writeSignedInt(stream, value)
```

#### 설명
각각 스트림에 부호 없는 4바이트 정수, 부호 있는 4바이트 정수를 출력할 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

`value`는 반드시 `int` 자료형이어야 합니다.

### `readLong`, `readSignedLong` 함수
#### 원형
```
func readLong(stream)
func readSignedLong(stream)
```

#### 설명
각각 스트림으로부터 부호 없는 8바이트 정수, 부호 있는 8바이트 정수를 입력 받을 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

함수의 반환값은 `long` 자료형입니다.

### `writeLong`, `writeSignedLong` 프로시저
#### 원형
```
proc writeLong(stream, value)
proc writeSignedLong(stream, value)
```

#### 설명
각각 스트림에 부호 없는 8바이트 정수, 부호 있는 8바이트 정수를 출력할 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

`value`는 반드시 `long` 자료형이어야 합니다.

### `readDouble` 함수
#### 원형
```
func readDouble(stream)
```

#### 설명
이 함수는 스트림으로부터 8바이트 IEEE754 수를 입력 받을 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

함수의 반환값은 `double` 자료형입니다.

### `writeDouble` 프로시저
#### 원형
```
proc writeDouble(stream, value)
```

#### 설명
이 프로시저는 스트림에 8바이트 IEEE754 수를 출력할 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

`value`는 반드시 `double` 자료형이어야 합니다.

### `readChar32` 함수
#### 원형
```
func readChar32(stream)
```

#### 설명
이 함수는 스트림으로부터 유니코드 Codepoint 하나를 입력 받을 때 사용합니다. 공백, 줄바꿈 문자는 입력 받지 않습니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

함수의 반환값은 `int` 자료형입니다.

### `writeChar32` 프로시저
#### 원형
```
proc writeChar32(stream, value)
```

#### 설명
이 프로시저는 스트림에 유니코드 Codepoint 하나를 출력할 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

`value`는 반드시 `int` 자료형이어야 합니다.

### `readString32` 함수
#### 원형
```
func readString32(stream)
```

#### 설명
이 함수는 스트림으로부터 UTF-32 문자열을 입력 받을 때 사용합니다. 공백, 줄바꿈 문자를 만나면 함수가 반환됩니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

함수의 반환값은 `string.String32` 자료형입니다. 문자열 사용을 마친 후 반드시 `string.destroy` 프로시저를 사용해 문자열을 파괴해야 합니다. 그렇지 않으면 메모리 누수가 발생합니다.

### `writeString32` 프로시저
#### 원형
```
proc writeString32(stream, valuePtr)
```

#### 설명
이 프로시저는 스트림에 UTF-32 문자열을 출력할 때 사용합니다.

`stream`은 반드시 `Stream` 구조체여야 합니다.

`valuePtr`은 반드시 `pointer` 자료형이어야 합니다. `valuePtr`이 가리키는 곳의 자료형은 반드시 `string.String32` 구조체여야 합니다.

## `string` 모듈
이 모듈은 문자열과 관련된 구조체와 함수를 제공합니다.

### `String32` 구조체
#### 원형
```
struct String32:
	pointer data
	long length
	long capacity
```

#### 설명
이 구조체는 UTF-32 문자열을 나타냅니다.

`data`는 `int` 자료형의 배열을 가리킵니다. 해당 배열에는 실질적인 문자열이 저장됩니다. 해당 배열에 접근하는 것은 가능하나, 이 필드의 값을 임의로 조작하지는 마세요.

`length`는 문자열의 길이를 저장합니다. 이 필드의 값을 임의로 조작하지 마세요.

`capacity`는 `data`가 가리키는 배열의 길이를 저장합니다. `capacity ≥ length`가 항상 성립합니다. 이 필드의 값을 임의로 조작하지 마세요.

### `create32` 함수
#### 원형
```
func create32
```

#### 설명
이 함수는 빈 UTF-32 문자열을 만들 때 사용합니다.

함수의 반환값은 `String32` 구조체입니다.

문자열 사용을 마친 후 반드시 `destroy` 프로시저를 사용해 문자열을 파괴해야 합니다. 그렇지 않으면 메모리 누수가 발생합니다.

### `push` 프로시저
#### 원형
```
proc push(strPtr, char)
```

#### 설명
이 함수는 문자열 맨 뒤에 문자를 삽입할 때 사용합니다. 목적지 문자열에 대한 포인터(`strPtr`)와 삽입할 문자(`char`)를 받습니다.

`strPtr`은 반드시 `pointer` 자료형이어야 합니다. `strPtr`이 가리키는 곳의 자료형은 반드시 `String32` 구조체여야 합니다.

`char`는 반드시 `int` 자료형이어야 합니다.

#### 예제
```
call create32
store myString

push 72 ; 'H'
lea myString
call push

push 105 ; 'i'
lea myString
call push

push 33 ; '!'
lea myString
call push ; myString = "Hi!"가 됩니다.
```

### `concat` 프로시저
#### 원형
```
proc concat(destPtr, srcPtr)
```

#### 설명
이 함수는 문자열 맨 뒤에 다른 문자열을 연결할 때 사용합니다. 목적지 문자열에 대한 포인터(`destPtr`)와 출발지 문자열에 대한 포인터(`srcPtr`)를 받습니다.

`destPtr`, `srcPtr`은 반드시 `pointer` 자료형이어야 합니다. `destPtr`, `srcPtr`가 가리키는 곳의 자료형은 반드시 `String32` 구조체여야 합니다.

#### 예제
```
; a = "Hello, "
; b = "world!"

lea b
lea a
call concat ; a = "Hello, world!"가 됩니다.
```

### `destroy` 프로시저
#### 원형
```
proc destroy(strPtr)
```

#### 설명
이 프로시저는 문자열을 파괴할 때 사용합니다.

`strPtr`은 반드시 `pointer` 자료형이어야 합니다. `strPtr`이 가리키는 곳의 자료형은 반드시 `String32` 구조체여야 합니다.