import pandas as pd

df = pd.read_parquet('/tmp/aidev_repo.parquet')
cpp = df[df['language'] == 'C++'].copy()
cpp['stars'] = pd.to_numeric(cpp['stars'], errors='coerce').fillna(0)
pool = cpp[cpp['stars'] >= 100].copy()

# deterministic shuffle
pool = pool.sample(frac=1.0, random_state=42).reset_index(drop=True)

with open('/tmp/cpp_pool_shuffled.txt', 'w') as f:
    for _, r in pool.iterrows():
        f.write("%s\t%d\n" % (r['full_name'], int(r['stars'])))

print("pool size (>=100 stars):", len(pool))
print("written /tmp/cpp_pool_shuffled.txt (seed=42)")
