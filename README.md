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

Также есть cli для компиляции входного файла в исполняемый файл:
```
./build/compiler_frontend_cli [-emit-llvm|-emit-obj|-emit-object] <input_file> [output_executable]
```

Пример кода в нынешней версии грамматики можно найти в [файле](example.cgor).
