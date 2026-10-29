import os
import random
import sys

FIRST_NAMES = [
    "Aarav", "Priya", "Rohan", "Ananya", "Vikram", "Sneha", "Karan", "Divya",
    "Aditya", "Pooja", "Rahul", "Neha", "Siddharth", "Kavya", "Arjun", "Ishita",
    "Varun", "Ritu", "Rajesh", "Swati", "Suresh", "Meera", "Manoj", "Deepika",
    "Gaurav", "Anjali", "Naveen", "Shreya", "Amit", "Bhavna", "Kunal", "Preeti",
    "Harish", "Pallavi", "Vivek", "Tanvi", "Sunil", "Aparna", "Alok", "Nandini"
]

LAST_NAMES = [
    "Sharma", "Iyer", "Verma", "Nair", "Malhotra", "Reddy", "Joshi", "Mehra",
    "Kulkarni", "Hegde", "Patel", "Gupta", "Chatterjee", "Menon", "Bose", "Deshmukh",
    "Chopra", "Rao", "Pillai", "Bhat", "Mukherjee", "Singh", "Das", "Saxena",
    "Pandey", "Mishra", "Banerjee", "Kapoor", "Agarwal", "Choudhury", "Pai", "Nambiar"
]

DEPARTMENTS = ["Engineering", "Sales", "Finance", "HR", "Marketing", "Operations"]

def generate_csv(filename, target_rows=100000):
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    print(f"Generating {target_rows:,} rows into {filename}...")
    
    random.seed(42)  # Deterministic generation for reproducible benchmarks
    
    with open(filename, "w", encoding="utf-8", newline="\n") as f:
        f.write("id,name,department,salary,age\n")
        for i in range(1, target_rows + 1):
            fname = random.choice(FIRST_NAMES)
            lname = random.choice(LAST_NAMES)
            name = f"{fname} {lname}"
            dept = random.choice(DEPARTMENTS)
            salary = random.randint(30, 180) * 1000
            age = random.randint(21, 62)
            f.write(f"{i},{name},{dept},{salary},{age}\n")
            
    filesize_mb = os.path.getsize(filename) / (1024 * 1024)
    print(f"Done! Created {filename} ({filesize_mb:.2f} MB, {target_rows:,} rows).")

if __name__ == "__main__":
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 100000
    out_path = sys.argv[2] if len(sys.argv) > 2 else "data/large_employees.csv"
    generate_csv(out_path, count)
