LOAD "data/employees.csv" AS emp_data;
SELECT name, department, salary, age;
FILTER department == "Engineering" AND salary > 75000;
AGGREGATE COUNT(*);
