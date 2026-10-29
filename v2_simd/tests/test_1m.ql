LOAD "data/employees_1m.csv" AS emp1m;
SELECT department, salary;
FILTER department == "Engineering";
AGGREGATE AVG(salary);
