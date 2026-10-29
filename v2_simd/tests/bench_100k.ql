LOAD "data/benchmark_100k.csv" AS dataset;
SELECT department, salary;
FILTER department == "Engineering";
AGGREGATE AVG(salary);
