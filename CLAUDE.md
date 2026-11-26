# OnlyDungeon - Claude Code Guidelines (Unreal Engine 5.9)

## 0. 프로젝트 개요 (고정 전제)
- **엔진 버전: Unreal Engine 5.9 고정.** `.uproject`의 `EngineAssociation`은 `"5.9"`이며, 엔진 업그레이드는 사용자 지시 없이 절대 하지 않는다. 다른 버전 전용 API나 문서를 근거로 코드를 작성하지 않는다.
- **장르:** 던전 액션 (쿼터뷰 필드 + 3인칭 보스전). **전투 구현이 프로젝트의 핵심**이다. 세부 기획은 아래 참고 문서를 따른다.
- **싱글플레이 전용:** 멀티플레이/네트워크 기능은 사용하지 않는다. (8장 참고)
- **전투 프레임워크:** Gameplay Ability System(GAS)을 사용한다. (6장 참고)
- **게임 모듈:** `OnlyDungeon` (Runtime). API 매크로는 `ONLYDUNGEON_API`.

### 참고 문서
- **기획서: `Docs/GameDesign.md`** — 게임 구조, 전투 모드, 장비, GAS 방향, 에셋 방침 등 모든 기획 결정의 기준이다.
  - 기획과 관련된 판단이 필요하면 이 문서를 먼저 읽는다.
  - 문서에 없거나 "미정"인 항목은 임의로 정하지 않고 사용자에게 묻는다.
  - 기획 변경은 사용자 승인 후에만 이 문서에 반영한다. CLAUDE.md에는 기획 내용을 적지 않는다.
- **진행 현황: `Docs/Progress.md`** — 완료·진행 중·다음 작업, 생성된 클래스와 에셋 목록, 작업 환경 노하우.
  - **새 대화를 시작하면 작업 전에 이 문서를 먼저 읽고 현재 상태를 파악한다.**
  - 기능 단위 작업이 끝날 때마다 이 문서를 갱신하고 같은 커밋에 포함한다.

---

## 1. 핵심 워크플로우: 설계 승인 게이트 (Human-in-the-Loop)
코드 작성 및 리팩토링 전, 다음에 해당하는 **중요한 게임 설계 및 아키텍처 결정**이 필요하면 **임의로 구현하지 말고 반드시 작업을 멈춘 뒤 사용자에게 먼저 질문하고 승인을 받는다.**

- **아키텍처 및 프레임워크:** `GameMode`, `PlayerController`, `PlayerState`, `Pawn/Character` 간 역할 분담, 모듈형 컴포넌트(`ActorComponent`) 구조 설계
- **GAS 구조:** `AbilitySystemComponent`의 소유 위치, `AttributeSet` 구성, Gameplay Tag 계층, 데미지 계산 방식(`ExecutionCalculation`/`ModMagnitudeCalculation`), 어빌리티-입력 바인딩 방식
- **데이터 흐름:** 서브시스템(`UGameInstanceSubsystem`, `UWorldSubsystem` 등) 도입, 델리게이트 연동 구조, DataAsset/DataTable 스키마
- **구현 방식 분기:** C++ 로직과 블루프린트(`UFUNCTION`, `UPROPERTY`) 노출 경계 설정
- **템플릿 코드 처리:** 템플릿 클래스의 이름 변경·삭제·대규모 수정 (9장 참고)

**질문 방식:** 직면한 과제 요약 -> 선택지(2~3개)와 장단점 -> 추천 방향 제시 후 대기

**설계 선행 (기능 구현·리팩토링 공통):** 바로 구현 코드를 작성하지 않고, 먼저 다음 세 가지를 요약해 승인을 받은 뒤 구현한다. (12장 지침 기준)
1. 아키텍처 및 데이터 레이아웃 (클래스·컴포넌트 배치, 소유 관계, 데이터가 어떤 컨테이너에 어떻게 놓이는지)
2. 핵심 인터페이스 초안 (공개 함수·델리게이트·UPROPERTY 시그니처)
3. 성능 병목과 엣지 케이스 대응 방안 (Hot Path 할당, 탐색 복잡도, 12-4의 상황별 정리)
- 오타·수치 조정·로그 추가처럼 구조가 바뀌지 않는 작은 수정은 예외로 바로 진행해도 된다.

---

## 2. MCP 에디터 조작 규칙 (Unreal MCP)
- **연결 정보:** `.mcp.json`의 `unreal-mcp` 서버 (`http://127.0.0.1:8000/mcp`). **언리얼 에디터가 실행 중일 때만 동작한다.** 연결에 실패하면 우회하지 말고, 에디터 실행 여부와 MCP 플러그인 활성화 여부를 확인해 달라고 사용자에게 요청한다.
- **조작 범위:** 액터 배치, 블루프린트 생성·수정, 에셋 생성, 레벨 편집 등 에디터 조작은 MCP로 자유롭게 수행할 수 있다. **단, 실행 전에 반드시 승인을 받는다.**
- **승인 요청 형식:** 실행 전에 다음 내용을 제시하고 대기한다.
  1. 작업 목적 (한 줄)
  2. 대상 레벨 / 에셋 경로
  3. 생성·수정·삭제할 항목 목록
  4. 되돌릴 수 있는지 여부
- **승인 범위:** 한 번 승인된 계획 범위 안의 작업은 연속으로 수행해도 된다. 계획을 벗어나는 작업이 생기면 다시 승인을 받는다.
- **파괴적 작업 경고:** `Content/`는 Git으로 추적하지 않으므로(3장) **에셋 삭제·덮어쓰기·이름 변경은 복구할 수 없다.** 이런 작업은 승인 요청 시 "복구 불가"를 명시한다.
- **에셋 경로:** 새 에셋은 `/Game/OnlyDungeon/` 아래에 만든다. 템플릿 에셋(`/Game/ThirdPerson`, `/Game/Characters`, `/Game/Variant_*` 등)은 직접 수정하지 않고, 필요하면 복제한 뒤 수정한다.
- **C++ 연동 순서:** 새 C++ 클래스를 부모로 하는 BP를 만들 때는 먼저 빌드 검증(10장)에 성공하고 에디터에 클래스가 반영된 것을 확인한 다음 MCP 작업을 시작한다.
- **작업 후 보고:** 실제로 변경된 에셋/액터 목록을 보고하고, 저장되지 않은 에셋이 있으면 알린다.

---

## 3. 버전 관리 규칙 (Source-Code Only Git Workflow)
Git 저장소는 용량 최적화와 속도를 위해 **'순수 텍스트(소스 코드) 전용'**으로 운영한다. 에이전트는 다음 사항을 반드시 준수한다.

- **추적 대상:** `Source/`, `Config/`, `.uproject`, `.Build.cs`/`.Target.cs`, `.gitignore`, `.gitattributes`, `.mcp.json`, `.vsconfig`, `ValidateBuild.bat`, `CLAUDE.md`, `README.md`, `Docs/`(기획 문서), `Tools/`(MCP 브리지 등 개발 도구)
- **에셋 추적 영구 금지:** 바이너리 에셋(`.uasset`, `.umap`, `.fbx`, `.png` 등)이 있는 `Content/` 폴더는 절대 스테이징(`git add`)하거나 추적하지 않는다. 에셋 조작이 필요하면 사용자에게 알린다.
- **빌드/임시 폴더 추적 금지:** `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`와 IDE가 생성하는 `*.sln`/`*.slnx`는 커밋하지 않는다.
- **`git add -A` / `git add .` 금지:** 변경한 파일만 경로를 지정해 스테이징하고, 커밋 전에 `git status`로 의도하지 않은 파일이 없는지 확인한다.
- **브랜치 구조 (Main-Dev):**
  - `dev`: 모든 개발과 테스트를 진행하는 기본 작업 브랜치. 커밋은 항상 `dev`에 한다.
  - `main`: 안정판. `dev`에서 빌드(`ValidateBuild.bat`)와 플레이 테스트가 끝나 문제가 없을 때만, **사용자 지시가 있으면** `dev`를 `--no-ff`로 병합한다. `main`에 직접 커밋하지 않는다.
  - 기능별 feature 브랜치는 만들지 않는다. 대신 커밋 단위와 접두사로 기능을 구분한다.
  - 작업 시작 전 `git branch --show-current`로 `dev`인지 확인한다.
- **원자적 커밋:** 한 번의 작업 단위에서는 하나의 기능이나 버그 수정만 커밋하고, 접두사로 종류를 반드시 명시한다. 커밋 메시지 본문은 한글로 작성해도 된다.
  - `Feat:` 새 기능 / `Fix:` 버그 수정 / `Refactor:` 동작 변화 없는 구조 개선 / `Chore:` 설정·빌드·도구 / `Docs:` 문서
  - 제목에 무엇을 했는지 구체적으로 쓴다. (예: `Feat: 스태미나 자연 회복 패시브 어빌리티 추가`)
- **푸시, 강제 푸시(`--force`), 히스토리 재작성:** 사용자가 명시적으로 지시한 경우에만 수행한다.

---

## 4. 네이밍 규칙 (Unreal 표준 + `OD` 접두사)
Epic 코딩 표준의 타입 접두사를 따르고, **이 프로젝트에서 새로 작성하는 코드에는 타입 접두사 바로 뒤에 `OD`(OnlyDungeon)를 붙인다.**

| 종류 | 형식 | 예시 |
|---|---|---|
| Actor 파생 | `AOD...` | `AODPlayerCharacter`, `AODEnemyBase` |
| UObject/Component 파생 | `UOD...` | `UODAbilitySystemComponent`, `UODAttributeSet_Combat`, `UODGameplayAbility` |
| 구조체 | `FOD...` | `FODDamageSpec` |
| 열거형 | `EOD...` (`enum class`, `uint8`) | `EODWeaponType` |
| 인터페이스 | `UOD...` / `IOD...` | `UODInteractable` / `IODInteractable` |
| 델리게이트 | `FOD...Signature` | `FODOnHealthChangedSignature` |
| 로그 카테고리 | `LogOD...` | `LogODCombat` |

- **파일명:** 클래스명에서 타입 접두사(A/U/F/E/I)만 뺀다. 예: `AODPlayerCharacter` -> `ODPlayerCharacter.h/.cpp`
- **변수/함수:** PascalCase. bool은 `b` 접두사(`bIsAttacking`). 멤버 변수에는 `OD`를 붙이지 않는다.
- **블루프린트/에셋:** 표준 에셋 접두사를 쓴다. `BP_`, `ABP_`(애님 BP), `WBP_`(위젯), `GA_`(어빌리티 BP), `GE_`(이펙트), `GC_`(큐), `IA_`/`IMC_`(입력), `DA_`/`DT_`(데이터), `AM_`(몽타주), `M_`/`MI_`(머티리얼), `SM_`/`SK_`(메시), `NS_`(나이아가라), `L_`(레벨). 에셋 이름에는 `OD`를 붙이지 않고, `/Game/OnlyDungeon/` 폴더로 구분한다.
- **Gameplay Tag:** `OD` 없이 계층형으로 작성한다. 예: `Ability.Attack.Light`, `State.Dead`, `Event.Montage.Hit`, `Data.Damage`.
- **C++ 폴더:** 새 코드는 `Source/OnlyDungeon/` 아래 기능별 폴더에 둔다. (예: `Core/`, `Character/`, `AbilitySystem/`, `AI/`, `UI/`, `Interaction/`) 처음 만드는 폴더는 구조 결정이므로 1장의 승인 대상이다.
- **include 경로:** 모듈 루트 기준 상대 경로를 쓴다. (예: `#include "AbilitySystem/ODAttributeSet_Combat.h"`) 새 폴더마다 `Build.cs`의 `PublicIncludePaths`에 경로를 추가하지 않는다.

---

## 5. Unreal Engine 5.9 표준 및 효율성 지향 코딩
UE 5.9의 최신 기능을 적극 활용하고, 실행 성능(CPU/메모리)을 고려한 Data-Oriented 방식을 지향한다.

- **최신 패러다임 우선:**
  - 헤더의 `UObject` 멤버 포인터는 반드시 `UPROPERTY()` + `TObjectPtr<T>`로 선언한다. `UPROPERTY`가 없으면 GC가 추적하지 않으므로 소유 참조는 항상 `UPROPERTY`를 붙인다.
  - **함수 인자, 반환값, 지역 변수는 원시 포인터(`T*`)를 쓴다.** (Epic 표준)
  - 레거시 시스템보다 최신 엔진 기능을 우선한다. 예: `Enhanced Input`, `Chaos Physics`, `StateTree`(AI), 전투 로직은 GAS
  - 거대한 상속 트리 대신 **`ActorComponent` + 인터페이스 조합**을 기본으로 한다. `GameFeatures`(Modular Gameplay Features) 플러그인 도입은 1장의 승인 대상이다.
- **성능 중심 효율성:**
  - **캐시 지역성:** 대량으로 반복 처리하는 데이터는 개별 `UObject` 할당보다 구조체 배열(`TArray<FStruct>`)을 우선 검토한다.
  - **컨테이너:** 크기를 예측할 수 있으면 `Reserve()`를 호출한다. 생성자 인자로 요소를 직접 만들 때는 `Emplace()`, 이미 만든 객체를 넣을 때는 `Add(MoveTemp(Value))`를 쓴다.
  - **Tick 최소화:** 새로 만드는 Actor/Component는 `PrimaryActorTick.bCanEverTick = false;` / `PrimaryComponentTick.bCanEverTick = false;`를 기본값으로 둔다. 매 프레임 갱신이 필요하면 먼저 `FTimerManager`, 델리게이트, GAS 이벤트(`GameplayEvent`, `AbilityTask`)로 대체할 수 없는지 검토한다. (`CharacterMovementComponent`, `AnimInstance`처럼 엔진이 원래 Tick하는 클래스는 예외)
  - **에셋 로딩:** 무거운 에셋 참조는 `TSoftObjectPtr`/`TSoftClassPtr`로 선언하고 필요할 때 비동기 로드한다.
- **메모리 및 GC 안전성:**
  - 소유하지 않는 약한 참조는 `TWeakObjectPtr<T>`를 쓴다.
  - `UObject` 포인터는 역참조 전에 단순 `nullptr` 비교 대신 **`IsValid(Obj)`**로 검증한다. (PendingKill/Garbage 상태 포함)
  - `TWeakObjectPtr`는 `.IsValid()` 또는 `.Get()` 결과로 검증하고, UObject가 아닌 포인터(`TSharedPtr`, 일반 C++ 포인터)는 `nullptr`/`IsValid()` 멤버 함수로 검증한다.
- **C++ / 블루프린트 경계 (기본값, 예외는 1장 승인 대상):**
  - 게임 로직, 데이터 구조, 계산은 C++에 둔다. 블루프린트는 에셋 조립, 수치 튜닝, 연출(몽타주·이펙트 연결)에 쓴다.
  - 노출은 최소 권한으로 한다. 기본은 `EditDefaultsOnly` / `BlueprintReadOnly`이고, 쓰기 권한(`BlueprintReadWrite`, `EditAnywhere`)은 이유가 있을 때만 준다.

---

## 6. GAS (Gameplay Ability System) 규칙
- **사전 설정 (최초 1회, 사용자 승인 후 진행):** `.uproject`에 `GameplayAbilities` 플러그인을 활성화하고, `OnlyDungeon.Build.cs`에 `GameplayAbilities`, `GameplayTags`, `GameplayTasks` 모듈을 추가한다.
- **싱글플레이 기준:** 리플리케이션 모드, 클라이언트 예측(Prediction Key), `NetExecutionPolicy` 튜닝은 신경 쓰지 않는다. 어빌리티의 `NetExecutionPolicy`는 기본값(`LocalPredicted`)을 그대로 둔다.
- **Attribute 변경 경로:** Attribute(체력, 스태미나 등)는 **반드시 GameplayEffect를 통해서만 변경한다.** C++이나 BP에서 직접 값을 대입하지 않는다. (초기화도 GE 또는 DataTable로)
- **데미지 처리:** 메타 Attribute(예: `IncomingDamage`)와 `ExecutionCalculation`으로 계산하고, `PostGameplayEffectExecute`에서 실제 체력에 반영한다. (최종 구조는 1장의 승인 대상)
- **Gameplay Tag:** C++에서 쓰는 태그는 Native Gameplay Tag(`UE_DECLARE_GAMEPLAY_TAG_EXTERN` / `UE_DEFINE_GAMEPLAY_TAG`)로 한곳(예: `Core/ODGameplayTags.h/.cpp`)에 모아 정의한다. 문자열 `FName`으로 태그를 직접 요청하지 않는다.
- **상태 판정:** 공격 중, 피격, 무적, 사망 같은 상태는 bool 멤버 변수 대신 Gameplay Tag(`State.*`)로 표현한다.
- **애니메이션 연동:** 공격 판정 타이밍은 AnimNotify -> `SendGameplayEventToActor` 또는 `AbilityTask_WaitGameplayEvent`로 연결한다.

---

## 7. 주석 및 로그 규칙
- **주석은 한글로 작성한다.** 헤더의 공개 API(클래스, `UFUNCTION`, `UPROPERTY`)에는 `/** */` 형식의 설명 주석을 단다. (에디터 툴팁으로 표시됨) 코드를 그대로 읽어 주는 주석은 쓰지 않고 "왜"를 설명한다.
- **소스 파일 인코딩:** 한글 주석이 들어간 `.h/.cpp`는 **UTF-8(BOM 포함)**으로 저장한다. 한국어 Windows(CP949)의 MSVC에서 BOM 없는 UTF-8 한글 주석은 C4819 경고나 줄 깨짐에 따른 컴파일 에러를 일으킬 수 있다.
- **전용 로그 카테고리:** 게임플레이 로그에는 `LogTemp`를 쓰지 않고 다음 카테고리를 쓴다. 정의 위치는 `Core/ODLogChannels.h/.cpp` (처음 필요할 때 생성)
  - `LogOD` : 일반 / 게임 흐름
  - `LogODCombat` : 전투, 데미지, 히트 판정
  - `LogODAbility` : GAS 어빌리티, 이펙트, Attribute
  - `LogODAI` : 적 AI, StateTree
  - 템플릿에 있는 `LogOnlyDungeon`은 템플릿 코드에서만 쓰고, 새 코드에서는 위 카테고리를 쓴다.
- **로그 레벨:** 매 프레임 또는 빈번한 로그는 `Verbose`/`VeryVerbose`, 비정상 상황은 `Warning`, 진행 불가 상황은 `Error`로 남긴다. 로그 메시지는 한글을 써도 된다.
- 개발용 검증은 `ensure()`/`ensureMsgf()`, 절대 일어나면 안 되는 조건은 `check()`를 사용한다.

---

## 8. 싱글플레이 전용 규칙
- `Replicated`/`ReplicatedUsing` 프로퍼티, `Server`/`Client`/`NetMulticast` RPC, `GetLifetimeReplicatedProps` 구현을 작성하지 않는다.
- `HasAuthority()` 분기, `IsLocallyControlled()` 같은 네트워크 분기 코드를 새로 추가하지 않는다. (템플릿 코드에 이미 있는 것은 그대로 둔다)
- 저장/로드는 `USaveGame` 기반 로컬 저장을 전제로 한다.
- 이 방침을 바꿀 때는 사용자 결정이 필요하다.

---

## 9. 템플릿 코드 취급
- `OnlyDungeonCharacter`, `OnlyDungeonGameMode`, `OnlyDungeonPlayerController`와 `Variant_Combat`, `Variant_Platforming`, `Variant_SideScrolling` 폴더는 **UE 템플릿 코드이며 참고용**이다.
- 새 기능은 템플릿 클래스를 확장하지 않고 `OD` 접두사 클래스로 새로 작성한다. 특히 템플릿 전투 인터페이스(`ICombatDamageable`, `ICombatAttacker`)는 GAS 데미지 흐름과 겹치므로 새 전투 코드에서 사용하지 않는다.
- 템플릿 클래스의 이름 변경이나 삭제는 BP 참조가 깨질 수 있으므로(`DefaultEngine.ini`의 `ActiveClassRedirects` 필요) 1장의 승인 대상이다.

---

## 10. 자동화된 빌드 검증 (Automated Build Verification)
C++ 소스 코드(`Source/`)나 `.Build.cs`/`.uproject`를 수정한 후, Git에 커밋하거나 사용자에게 완료를 보고하기 전에 **반드시 다음 스크립트로 컴파일 에러가 없는지 스스로 검증한다.**

- **실행 명령어 (프로젝트 루트에서):** `cmd /c ValidateBuild.bat` (PowerShell에서는 `.\ValidateBuild.bat`)
- **빌드 대상:** `OnlyDungeonEditor` / `Win64` / `Development`
- **결과 코드:**
  - `0` = `[SUCCESS]` 빌드 성공
  - `1` = `[FAIL]` 컴파일/링크 에러 -> 에러 핸들링 루프 진행
  - `2` = `[ENV]` 엔진 경로를 찾지 못하는 등 환경 문제 -> 루프 없이 즉시 사용자에게 보고
  - `3` = `[LIVECODING]` 에디터의 Live Coding이 활성화된 상태 -> 루프 없이 사용자에게 에디터 종료 또는 에디터 내 Live Coding 컴파일(`Ctrl+Alt+F11`)을 요청
- **전체 로그 위치:** `Saved/ValidateBuild/LastBuild.log` (콘솔 출력이 잘렸을 때 참고)
- **에러 핸들링 루프:**
  1. 결과가 `1`이면 로그에서 **첫 번째 에러부터** 분석한다. (뒤쪽 에러는 앞 에러의 연쇄인 경우가 많다)
  2. 원인 코드를 수정하고 `ValidateBuild.bat`을 다시 실행한다.
  3. `[SUCCESS]`가 나올 때까지 반복하되, **최대 5회**까지만 시도한다. 5회 안에 해결되지 않으면 멈추고 로그 요약과 함께 보고한다.
- **금지 사항:** 빌드를 통과시키려고 기능 코드를 삭제·주석 처리하거나, `#pragma warning(disable)` 등으로 경고를 억제하거나, 다른 파일의 무관한 코드를 수정하지 않는다.
- **예외 상황:** 코드 수정만으로 해결되지 않는 링커 에러, UHT(`.generated.h`) 에러, 엔진 내부 매크로 모호성 에러가 발생하면 루프를 중단하고 로그와 함께 사용자에게 도움을 요청한다.
- **헤더 변경 주의:** `UCLASS`/`USTRUCT`/`UPROPERTY`/`UFUNCTION` 추가·변경이나 새 클래스 추가는 Live Coding으로 반영되지 않을 수 있다. 빌드 성공 후 에디터 재시작이 필요하면 사용자에게 알린다.

---

## 11. 캐릭터·애니메이션 점검: 두 발 고정(미끄러짐)
새 캐릭터(플레이어 외형·몬스터)를 넣거나 이동·공격 모션을 바꿀 때는 **"두 발이 땅에 고정된 채 몸만 미끄러지는" 문제를 반드시 점검**하고, 결과(보폭·slip 수치)를 완료 보고에 포함한다. 이 프로젝트에서 반복해서 발생한 문제다(구울 걷기, 달리기 모션, 역병의 족장).

- **원인별 확인 항목:**
  1. **리타깃 모션의 IK 본:** 리타깃 결과는 `ik_foot_l/r`·`ik_hand_*`가 기준 자세로 고정된다. 발 IK(컨트롤 릭)가 연결된 ABP에서 쓰는 대기·이동 모션은 `foot_l/r` 궤적을 `ik_foot_l/r`에 구워 넣거나, 컨트롤 릭을 출력에서 분리한다.
  2. **가산(additive) 대기 모션:** Paragon 팩 등의 `Idle`이 가산 포즈일 수 있다. `additive_anim_type`이 `AAT_NONE`인지 확인한다. (가산이면 메시가 무너져 안 보인다)
  3. **ABP의 점프·낙하·착지 상태:** 착지 상태는 시퀀스가 끝나야 빠져나오므로 짧은 모션을 넣는다. 긴 대기 모션을 넣으면 그동안 서 있는 자세로 미끄러진다.
  4. **블렌드스페이스 속도 샘플:** 실제 이동 속도(추격·간 보기·순찰)마다 모션 보폭 속도와 맞는 샘플이 있어야 한다. 대기(0)와 빠른 조깅만 있으면 느린 이동에서 보폭이 작아 미끄러진다. 걷기 모션이 없으면 같은 모션을 느린 재생 속도로 낮은 속도에 둔다.
  5. **전진하는 공격(StepDistance):** 제자리 모션(특히 공중 공격·서서 휘두르는 모션) 동안 크게 전진하면 발이 고정된 채 미끄러진다. 달리기·점프(다리를 접는) 구간에만 전진시키고, 재생 속도로 보폭 속도와 전진 속도를 맞춘다.
- **측정:** PIE에서 `Tools/AnimCheck/od_foot_check.py`의 `check()`로 추격·간 보기·전진 공격마다 보폭과 slip(디딤 발 속도 ÷ 몸 속도)을 잰다. 플레이어 조깅(slip 약 0.4)과 비슷해야 하며, 보폭이 거의 0이거나 slip이 0.8 이상이면 위 원인을 찾아 고친다. 수치와 함께 옆에서 찍은 캡처로 다리가 실제로 움직이는지 확인한다.


---

## 12. 설계 기본 지침 (아키텍처·성능·안전)
새 코드와 리팩토링 모두 이 지침을 기본값으로 삼는다. 지침을 어겨야 하는 경우(엔진 API 제약 등)는 이유를 주석으로 남기고 설계 요약(1장)에 적는다.

### 12-1. 의존성 규칙
- **방향:** `UI` → `Character`/`AbilitySystem`/`AI`/`World` → `Core` 순으로만 의존한다. 게임플레이 코드(`Core`, `Character`, `AbilitySystem`, `AI`, `Equipment`, `World`)는 `UI/` 헤더를 include하지 않는다.
- **통신:** 하위 계층이 상위 계층(UI·연출)에 알릴 때는 델리게이트(`DECLARE_MULTICAST_DELEGATE_*` / `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*`), GAS Attribute 변경 델리게이트, GameplayCue, GameplayEvent를 쓴다. 위젯은 스스로 구독하고 `NativeDestruct`에서 해제한다.
- 위젯이 액터를 받아야 하면 `Core/`에 둔 인터페이스로 넘기고, 게임플레이 쪽은 구체 위젯 클래스를 모른다.

### 12-2. 성능·메모리 제약
- **Hot Path 정의:** Tick, `NotifyTick`, StateTree `Tick`, 반복 타이머(0.5초 이하 주기), 어빌리티의 주기 갱신 함수.
- **Hot Path 안에서 힙 할당·컨테이너 재할당 금지:**
  - 매 호출 쓰는 임시 배열은 멤버 스크래치 버퍼로 두고 `Reset()`으로 재사용한다. 크기는 초기화 시점에 `Reserve()`한다.
  - 크기 상한이 작고 정해진 지역 배열은 `TArray<T, TInlineAllocator<N>>`를 쓴다.
  - `FString::Printf`/`FText::Format` 같은 문자열 생성은 값이 바뀐 경우에만 한다. (디버그 표시 제외)
  - `TArray`를 값으로 반환하는 함수를 Hot Path에서 호출하지 않는다. 순회가 필요하면 `ForEach...(TFunctionRef)` 형태나 const 참조 반환을 쓴다.
- **연속 메모리:** 순회 대상 데이터는 `TArray<FStruct>`(연속 메모리)로 구성한다. 개별 `UObject`를 만들어 순회하지 않는다.
- **Tick:** 기본 꺼둠(5장). 꼭 필요하면 `bStartWithTickEnabled = false`로 두고 필요한 동안만 `SetComponentTickEnabled`/`SetActorTickEnabled`로 켠다. 짧은 주기 갱신은 타이머를 쓰되 주기는 이름 있는 상수로 둔다.
- **탐색 복잡도:** 탐색은 O(1) 또는 O(log N) 이내로 한다.
  - 컴포넌트·서브시스템은 매번 `FindComponentByClass`/`GetSubsystem`으로 찾지 말고 소유 클래스의 게터나 캐시된 `TWeakObjectPtr`를 쓴다.
  - 특정 종류의 액터 목록은 `TActorIterator`/`GetAllActorsOfClass` 대신 서브시스템에 등록(`Register/Unregister`)해 관리한다.
  - 키 검색이 잦은 컬렉션은 `TMap`/`TSet`을 쓴다. 단, 원소 수 상한이 작은(약 16개 이하) 배열의 선형 검색은 해시보다 빠르므로 허용한다.

### 12-3. 금지 패턴 (Anti-Patterns)
- **원시 포인터로 소유권 관리 금지:** 소유는 `UPROPERTY() TObjectPtr`, 비소유는 `TWeakObjectPtr`, 비 UObject는 `TUniquePtr`/`TSharedPtr`. `new`/`delete` 직접 사용 금지.
- **매직 넘버 하드코딩 금지:** 튜닝 값은 `UPROPERTY(EditDefaultsOnly)` 또는 DataAsset으로, 고정 상수는 파일 상단 `namespace` 안의 `constexpr`로 이름을 붙인다. (0, 1, 0.5 같은 수학 상수와 `KINDA_SMALL_NUMBER` 류는 예외)
- **존재하지 않는 엔진 API 추측 작성 금지:** 확실하지 않은 API는 엔진 소스(`Engine/Source`)나 기존 프로젝트 코드에서 시그니처를 확인한 뒤 쓴다. 확인할 수 없으면 사용자에게 알린다.
- 문자열 `FName`으로 태그·소켓·본을 매번 생성하지 않는다. 태그는 Native Tag(6장), 소켓·본 이름은 `UPROPERTY` 또는 `static const FName`.

### 12-4. 반드시 처리할 엣지 케이스
- **스킬 시전 중 피격·사망:** 사망 시 `Ability.Action` 계열 어빌리티를 취소하고, 각 어빌리티의 `EndAbility`는 취소(`bWasCancelled`) 경로에서도 타이머·스폰한 컴포넌트·이동 설정·부여한 태그를 모두 원상복구한다. 피격 경직(`State.Stunned`)이 행동을 끊는 경우도 같다.
- **중복 실행 방지:** 같은 요청이 연달아 들어와도 한 번만 처리한다. 어빌리티는 `ActivationBlockedTags`/`ActivationOwnedTags`, 일반 로직은 상태 태그나 진행 중 플래그로 재진입을 막는다. 타이머는 재설정 전에 기존 핸들을 정리한다.
- **Null/Invalid 대상 방어:** 타이머·델리게이트 콜백, 지연 실행 람다는 실행 시점에 대상을 `IsValid()`/`TWeakObjectPtr::Get()`으로 다시 검증한다. 람다는 `CreateWeakLambda`/`CreateUObject`로 묶는다.
- **수명 정리:** `EndPlay`/`NativeDestruct`/`Deinitialize`에서 타이머를 지우고 바인딩한 델리게이트를 해제한다.
