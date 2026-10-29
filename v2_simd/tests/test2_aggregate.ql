LOAD "data/employees.csv" AS emp_data;
SELECT name, salary;
FILTER salary >= 50000;
AGGREGATE AVG(salary);
