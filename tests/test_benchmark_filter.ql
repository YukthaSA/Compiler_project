LOAD "data/large_employees.csv" AS large_emp;
SELECT name, department, salary;
FILTER salary > 100000;
