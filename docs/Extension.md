# ShitAsm 확장 기능
- 개발자의 편의를 위해 ShitBC 스펙에는 없지만 ShitAsm에서 자체적으로 제공하는 기능입니다.

## 목차
- [`string32` 구문](#string32-구문)

## `string32` 구문
### 원형
```
string32 "문자열" to 변수이름
```

### 설명
특정 문자열을 저장하고 있는 `/std/string.sba` 모듈의 `String32` 구조체를 생성하는 구문입니다. 매개 변수 또는 지역 변수에 구조체를 저장합니다. 문자열 사용을 마친 후 반드시 `/std/string.sba` 모듈의 `destroy` 프로시저를 사용해 문자열을 파괴해야 합니다. 그렇지 않으면 메모리 누수가 발생합니다.

사용을 위해 반드시 `/std/string.sba` 모듈을 미리 `import` 해야 합니다.

### 예제
```
import "/std/io.sba" as io
import "/std/string.sba" as str

proc entrypoint:
    string32 "Hello, world!" to string
    lea string
    call io.getStdout
    call io.writeString32

    lea string
    call str.destroy
```