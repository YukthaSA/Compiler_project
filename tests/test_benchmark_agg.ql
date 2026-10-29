LOAD "data/large_employees.csv" AS large_emp;
SELECT department, salary;
FILTER department == "Engineering";
AGGREGATE AVG(salary);
