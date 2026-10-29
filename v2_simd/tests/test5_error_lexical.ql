LOAD "data/employees.csv" AS emp_data;
SELECT name, #invalid_char, salary;
FILTER salary > 50000;
