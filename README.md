# ShitAsm
**ShitBC 어셈블러:** ShitBC 어셈블리를 ShitVM 바이트 파일로 만들어줍니다.

## 컴파일
```
$ git clone --recurse-submodules https://github.com/ShitVM/ShitAsm.git
$ cd ShitAsm
$ cmake CMakeLists.txt
$ make
```

## 사용법
```
$ cd bin
$ ./ShitAsm <입력: ShitBC 어셈블리 경로> [출력: ShitVM 바이트 파일 경로]
```
ShitVM 바이트 파일 경로를 지정하지 않으면 `ShitAsmOutput.sbf` 파일에 결과가 저장됩니다.

## 문법
- ShitBC 어셈블리의 권장 확장자는 `*.sba` 입니다.

### 프로시저, 함수, 레이블
ShitBC 어셈블리는 프로시저와 함수로 구성됩니다. 프로시저는 반환 값이 없는 명령어의 집합을, 함수는 반환 값이 있는 명령어의 집합을 나타냅니다.

```
proc/func 이름:
	...
```
프로시저를 만들 때에는 proc 키워드를, 함수를 만들 때에는 func 키워드를 사용합니다. 이름은 중복될 수 없으며, 언더바(`_`)를 제외한 특수문자를 사용할 수 없습니다. 단, 숫자는 사용할 수 있습니다.

```
proc entrypoint:
	...
```
특히, 이름이 entrypoint인 프로시저를 특별히 진입점이라고 합니다. ShitVM에 의해 프로그램이 실행되면 진입점이 가장 먼저 실행됩니다. 진입점이 없으면 프로그램은 정상적으로 동작하지 않습니다.

```
proc/func 이름(a, b, c, ...):
	...
```
프로시저와 함수는 매개변수를 가질 수도 있습니다. 이름 뒤에 소괄호 쌍을 붙인 뒤, 소괄호 안에 컴마(`,`)로 매개 변수를 구분합니다. 단, 진입점은 매개변수를 가질 수 없습니다.

```
proc/func 이름:
	...
foo:
	...
bar:
	...
```
프로시저와 함수 안에는 레이블을 만들 수 있습니다. 이름 뒤에 콜론(`:`)을 붙이기만 하면 됩니다. 레이블의 이름은 중복될 수 없으나, 다른 프로시저/함수에 있는 레이블의 이름을 사용할 수는 있습니다. 레이블은 각각의 프로시저/함수마다 별도로 분리되어 만들어지므로 다른 프로시저/함수에 있는 레이블로 이동할 수는 없습니다.

### 명령어의 기본 구조
```
<니모닉> [피연산자]
```
모든 명령어는 기본적으로 니모닉을 갖습니다. 니모닉은 해당 명령어가 어떤 동작을 수행해야 하는지 나타냅니다. 니모닉에 따라 피연산자가 필요한 경우도 있습니다. 이럴 때에는 니모닉과 띄어쓰기 등으로 구분을 한 뒤 적습니다.

```
push 1024			; Error!

proc entrypoint:
	push 1024		; OK
	...
```
또, 모든 명령어는 반드시 어떤 프로시저나 함수에 소속되어 있어야 합니다. 프로시저나 함수에 소속되어 있지 않은 명령어가 있다면 ShitBC 어셈블러는 오류를 발생시키고, ShitVM 바이트 파일을 생성하지 않습니다.

### 스택과 지역 변수
ShitVM은 스택 기반 가상머신으로, 연산에 필요한 피연산자들을 스택에 저장한 뒤 연산을 하고, 그 결과물을 다시 스택에 저장하는 구조로 되어 있습니다.

```
push 1024
push 1024i
push 1024l
push 1024.0
```
`push` 니모닉을 사용하면 스택에 연산에 필요한 상수들을 저장할 수 있습니다. 이 니모닉은 피연산자가 필요한데, 스택에 저장할 상수가 피연산자가 됩니다. 이 명령어가 실행되면 피연산자가 스택의 가장 위에 저장됩니다.

ShitVM에서 다루는 값들은 자료형이 정해져 있는데, 자료형은 어떠한 값이 어떤 종류인지, 어떤 특성을 갖는지, 크기는 어느 정도 되는지 나타냅니다. ShitVM에서 기본적으로 제공하는 자료형의 종류와 설명은 다음과 같습니다.

|이름|크기|설명|표현 범위|
|:-:|:-:|:-:|:-:|
|`int`|4바이트|가장 기본적인 정수형입니다.|0\~4,294,967,295<br>-2,147,483,648\~2,147,483,647|
|`long`|8바이트|크기가 큰 정수형입니다.|0\~18,446,744,073,709,551,615<br>-9,223,372,036,854,775,808\~9,223,372,036,854,775,807|
|`double`|8바이트|가장 기본적인 실수형입니다.|유효숫자 15자리|

ShitVM에서 정수형은 기본적으로 unsigned로 처리됩니다. 따라서 부호의 여부에 영향을 받는 일부 동작들은 부호를 구분하는 니모닉과 구분하지 않는 니모닉으로 나뉘어 있습니다. 또, 정수형은 2의 보수법, 실수형은 IEE754에 따라 구현됩니다.

기본적으로 `push` 니모닉을 사용하면 피연산자의 크기에 따라 자동으로 자료형이 결정됩니다. 소수점이 있으면 `double`, `int`로 표현할 수 있다면 `int`, 그 외에 경우에는 `long`으로 결정됩니다. 그러나 정수 상수의 자료형을 직접 지정하고 싶을 경우에는 `i` 또는 `l` 접미사를 사용합니다. `i` 접미사를 사용하면 해당 상수의 타입은 `int`가 되며, `l` 접미사를 사용하면 `long`이 됩니다.

```
pop
```
스택에 저장된 값을 삭제하고 싶다면 `pop` 니모닉을 사용하면 됩니다. 자료형에 관계없이 스택의 가장 위에 있는 값을 1개 삭제합니다. 이 니모닉은 피연산자가 필요하지 않습니다.

```
push 0
store i		; i = 0
push 1
store j		; j = 1
load j
store i		; i = 1
```
ShitVM에서는 지역 변수도 사용할 수 있습니다. 지역 변수는 스택에 저장되며, `store` 니모닉을 사용하면 지역 변수를 새롭게 만들 수 있습니다. 이 니모닉은 피연산자가 필요한데, 피연산자를 이름으로 하는 지역 변수를 만듭니다. 지역 변수의 값은 스택의 가장 위에 있는 값이 됩니다. 지역 변수의 값을 스택으로 가져오고자 한다면 `load` 니모닉을 사용하면 됩니다. 마찬가지로 피연산자는 지역 변수의 이름이 됩니다.

또, `store` 니모닉은 지역 변수를 만드는 역할 뿐만이 아니라 이미 존재하는 지역 변수에 새로운 값을 저장하는 동작도 수행합니다. 즉, 피연산자가 존재하지 않는 지역 변수라면 새 지역 변수를 만들고, 존재하는 지역 변수라면 해당 지역 변수에 값을 새롭게 저장합니다.

```
i2l
i2d
l2i
l2d
d2i
d2l
```
스택에 있는 값의 자료형을 변경할 수도 있습니다. 이를 형 변환이라고 합니다. 형 변환 니모닉의 종류와 설명은 다음과 같습니다.

|니모닉|시작 자료형|변환 자료형|
|:-:|:-:|:-:|
|`i2l`|`int`|`long`|
|`i2d`|`int`|`double`|
|`l2i`|`long`|`int`|
|`l2d`|`long`|`double`|
|`d2i`|`double`|`int`|
|`d2l`|`double`|`long`|

형 변환 니모닉들은 피연산자가 필요하지 않으며, 스택의 가장 위에 있는 값의 자료형을 바꾸게 됩니다.

### 산술 연산, 비트 연산
스택에 있는 값끼리 산술 연산을 하려면 다음 니모닉을 사용합니다.

|니모닉|설명|스택의 최소 크기|
|:-:|:-:|:-:|
|`add`|덧셈|2|
|`sub`|뺄셈|2|
|`mul`|부호 없는 곱셈|2|
|`imul`|부호 있는 곱셈|2|
|`div`|부호 없는 나눗셈|2|
|`idiv`|부호 있는 나눗셈|2|
|`mod`|부호 없는 나머지 연산|2|
|`imod`|부호 있는 모듈로 연산|2|
|`neg`|부호 변경|1|
|`inc`|1 증가|1|
|`dec`|1 감소|1|

스택에 있는 값끼리 비트 연산을 하려면 다음 니모닉을 사용합니다.

|니모닉|설명|스택의 최소 크기|
|:-:|:-:|:-:|
|`and`|논리곱|2|
|`or`|논리합|2|
|`xor`|배타적 논리합|2|
|`not`|비트 반전|2|
|`shl`|비트 왼쪽으로 시프트|2|
|`sal`|비트 왼쪽으로 시프트|2|
|`shr`|부호 없는 비트 오른쪽으로 시프트|2|
|`sar`|부호 있는 비트 오른쪽으로 시프트|2|

이 니모닉들은 스택에 저장된 값을 피연산자로 하므로 별도의 피연산자를 코드에 명시할 필요는 없습니다. 피연산자의 순서가 중요한 이진 연산은 스택의 가장 위에 있는 값을 오른쪽 피연산자, 그 값의 아래에 있는 값을 왼쪽 피연산자로 해 연산합니다.

연산의 결과는 피연산자가 스택에서 자동으로 삭제된 후 스택에 저장되며, 피연산자의 자료형은 반드시 같아야 합니다.

### 비교 연산, 분기
```
cmp
icmp
```
스택에 있는 값끼리 비교를 하려면 `cmp` 또는 `icmp` 니모닉을 사용합니다. 유추할 수 있듯이 `cmp`는 부호가 없는 비교, `icmp`는 부호가 있는 비교입니다. 왼쪽 피연산자가 오른쪽 피연산자보다 더 크면 1, 더 작으면 -1, 두 값이 같으면 0이 결과가 되며, 산술/비트 연산과 같이 피연산자는 스택에서 자동으로 삭제된 후 결과가 스택에 저장됩니다. 피연산자의 자료형도 마찬가지로 같아야 합니다. 결과의 자료형은 반드시 `int`입니다.

`cmp` 또는 `icmp` 니모닉으로 비교한 결과를 바탕으로 원하는 레이블로 이동하려면 조건 분기 니모닉을 사용합니다.

|니모닉|조건|스택의 값|
|:-:|:-:|:-:|
|`je`|`a == b`|0|
|`jne`|`a != b`|0이 아님|
|`ja`|`a > b`|1|
|`jae`|`a >= b`|-1이 아님|
|`jb`|`a < b`|-1|
|`jbe`|`a <= b`|1이 아님|

조건 분기에 성공하면 비교 결과는 스택에서 삭제되며, 비교 결과의 자료형이 반드시 `int`일 필요는 없습니다. 조건에 관계없이 분기하려면 `jmp` 니모닉을 사용하면 됩니다. 이 니모닉은 비교 결과를 필요로 하지 않습니다.

분기 명령어들은 반드시 피연산자가 필요한데, 이동할 레이블의 이름이 피연산자가 됩니다. 레이블은 반드시 해당 명령어가 소속된 프로시저/함수 내에 있는 레이블이어야 합니다.

### 호출과 반환
```
call <프로시저/함수 이름>
```
프로시저나 함수를 호출할 때에는 분기 명령어들을 사용하지 않고 `call` 니모닉을 사용합니다. 분기 명령어와 마찬가지로 피연산자가 필요한데, 대신 호출할 프로시저 또는 함수의 이름을 피연산자로 합니다.
```
push 1
push 0
call foo
```
호출할 프로시저 또는 함수가 매개 변수를 갖고 있다면 호출하기 전에 매개 변수의 개수에 맞게 인수들을 스택에 저장하면 됩니다. 이때 인수들은 스택에 저장한 순서의 역순으로 전달됩니다.
```
proc/func 이름:
	...
	ret
```
프로시저나 함수는 반드시 종료할 때 `ret` 니모닉을 사용해야 합니다. 피연산자는 필요하지 않으며, 해당 명령어는 매개 변수들을 정리하고 호출한 곳으로 다시 돌아가는 동작을 수행합니다.