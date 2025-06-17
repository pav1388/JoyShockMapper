def sync_spaces(file1_path, file2_path):
    # Читаем файлы, сохраняя все символы
    with open(file1_path, 'r', encoding='utf-8') as f1:
        file1_lines = f1.readlines()  # Сохраняем \n в конце
    
    with open(file2_path, 'r', encoding='utf-8') as f2:
        file2_lines = f2.readlines()
    
    if len(file1_lines) != len(file2_lines):
        print("Error: Translation files have different number of lines")
        return False
    
    modified_lines = []
    
    for i in range(len(file1_lines)):
        line1 = file1_lines[i]
        line2 = file2_lines[i]
        
        # Определяем пробелы в начале file1
        leading_spaces = len(line1) - len(line1.lstrip(' '))
        
        # Определяем пробелы в конце file1 (до \n если есть)
        stripped_line1 = line1.rstrip('\n')
        trailing_spaces = len(stripped_line1) - len(stripped_line1.rstrip(' '))
        
        # Получаем содержимое строки из file2 (без начальных и конечных пробелов)
        content = line2.strip(' \n')
        
        # Формируем новую строку
        new_line = (' ' * leading_spaces) + content + (' ' * trailing_spaces)
        
        # Восстанавливаем \n если он был в оригинале
        if line2.endswith('\n'):
            new_line += '\n'
        
        modified_lines.append(new_line)
    
    # Записываем результат
    with open(file2_path, 'w', encoding='utf-8') as f2:
        f2.writelines(modified_lines)
    
    return True


def main():
    sync_spaces('translation-en.txt', 'translation-ru.txt')
    print(f"\nCompleted")
    

if __name__ == "__main__":
    main()
    input("\nPress Enter to continue...")