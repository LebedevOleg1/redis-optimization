import matplotlib.pyplot as plt
import numpy as np
import argparse
from scipy.signal import medfilt

def main():
    # Парсинг аргументов командной строки
    parser = argparse.ArgumentParser(description="Отрисовка времени отклика с сглаживанием.")
    parser.add_argument('--window', type=int, default=100, help='Размер окна для сглаживания (нечетное для медианы)')
    parser.add_argument('--method', type=str, choices=['mean', 'median'], default='mean', help='Метод сглаживания')
    parser.add_argument('--output', type=str, default=None, help='Имя файла для сохранения графика')
    args = parser.parse_args()

    # Чтение данных из файла
    with open('response_times.txt', 'r') as f:
        times = np.array([float(line.strip()) for line in f])

    # Убедимся, что размер окна не превышает длину данных
    window = min(args.window, len(times))

    # Применение сглаживания
    if args.method == 'mean':
        smoothed = np.convolve(times, np.ones(window)/window, mode='same')
    elif args.method == 'median':
        if window % 2 == 0:
            window += 1
            print(f"Размер окна увеличен до {window}, чтобы он был нечетным для медианного фильтра")
        smoothed = medfilt(times, kernel_size=window)

    # Построение графика
    x = np.arange(len(times))
    plt.figure(figsize=(12, 6))
    plt.plot(x, times, label='Исходные данные', alpha=0.1)
    plt.plot(x, smoothed, label=f'Сглаженные данные ({args.method}, window={window})', color='red')
    plt.xlabel('Номер запроса')
    plt.ylabel('Время отклика (мс)')
    plt.title('Время отклика на /article/random')
    plt.legend()
    plt.grid(True)

    # Сохранение или отображение графика
    if args.output:
        plt.savefig(args.output)
    else:
        plt.show()

if __name__ == "__main__":
    main()