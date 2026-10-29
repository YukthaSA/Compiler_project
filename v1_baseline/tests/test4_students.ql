LOAD "data/students.csv" AS student_records;
SELECT name, branch, gpa;
FILTER gpa >= 8.5;
AGGREGATE MAX(gpa);
