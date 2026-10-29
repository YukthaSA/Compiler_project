LOAD "data/employees.csv" AS emp_data;
SELECT name, department, salary;
FILTER salary > 60000;
