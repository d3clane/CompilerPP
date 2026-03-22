# CompilerPP

## Build and run

```
git clone https://github.com/d3clane/CompilerPP.git
cmake -S . -B build/
cmake --build build/
```

Запуск тестов:
```
./build/compiler_frontend_tests
```
или
```
ctest --test-dir build --output-on-failure   
```

Также есть cli для интерпретации входного файла:
```
./build/compiler_frontend_cli <input_file>
```

Пример кода в нынешней версии грамматики можно найти в [файле](example.cgor).
