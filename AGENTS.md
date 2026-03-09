# SVision

## Build

1. C++ 20 code.
2. Build system is conan+cmake (note that presets might change between systems).
3. Configure:
```
conan install . -s build_type=Debug --build=missing
cmake --preset conan-release -G Ninja
```
3. Build:
```
cmake --preset conan-debug -G Ninja
```
4. LLMs should only deal with debug builds.

## Coding standards

1. General
    1. C++ 20 + conan for external dependencies.
    1. Prefer readabilty over hacks.
    1. No platform dependent code, when needed abstract to a virtual interface.
    1. Coments should explain "why" not "what".
    1. Avoid excesive comments. Code should be clear enough.
    1. Comments before statment, not aside it.
    1. Do not abuse with icons on comments.
    1. File names are *.cpp, *.hpp (*.mm where needed)
    1. First 2 lines will be: `// SPDX-License-Identifier: MIT` and `// SPDX-FileCopyrightText: `
    1. Includes will be guarded by `#pragma once`.
    1. Do not modify `todo.txt`
    1. When working on a widget, do not modify platform code - keep modifications
       inside the widget code. If platform changes are needed - prompt developer
       to modify those things.
1. Variables
    1. Variable names are `snake_case`.
    2. Macros (`#define`) are always UPPER_CASE.
    3. Members will not have a `_` prefix of suffix, nor `m_` prefix.
    4. Pass arguments as `const &`, unless you need to modify or pointers
       or trivial types (int, float etc).
    5. Variables are always `auto`.
1. Functions
    1. Stand alone functions will have trailing return syntax (`auto foo() -> bool`)
    2. If functions are used only in current compilation unit, make them static.
    3. If a method/function needs a callback function, it will
1. Blocks
    1. All blocks will have `{}`, even single line `if`/`for`/`while` loops.
    1. Try to exit from block on errors, edge cases at beginning of blocks
    1. Do not nest more than 5 blocks - reduce right indentation.
1. Classes
    1. Class names are `PascalNotation`.
    2. Methods are `camelCase`.

