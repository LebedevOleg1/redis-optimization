import matplotlib.pyplot as plt

with open('response_times.txt', 'r') as f:
    times = [int(line.strip()) for line in f]

# Разделяем на кэшированные и некэшированные (примерный порог 50 мс)
cached = [t for t in times if t < 50]
uncached = [t for t in times if t >= 50]

plt.hist(uncached, bins=20, alpha=0.5, label='Без кэширования')
plt.hist(cached, bins=20, alpha=0.5, label='С кэшированием')
plt.xlabel('Время отклика (мс)')
plt.ylabel('Частота')
plt.title('Сравнение времени отклика на /article/random')
plt.legend()
plt.show()