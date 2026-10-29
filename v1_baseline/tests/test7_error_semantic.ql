LOAD "data/employees.csv" AS emp_data;
SELECT name, bonus_percentage;
FILTER salary > 50000;
AGGREGATE AVG(department);
