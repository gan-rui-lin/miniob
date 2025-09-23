CREATE TABLE date_table(id int, u_date date);
INSERT INTO date_table VALUES (1,'2020-01-21');
INSERT INTO date_table VALUES (2,'2020-1-1');
SELECT * FROM date_table;
SELECT * FROM date_table WHERE u_date>'2020-1-20';
DELETE FROM date_table WHERE u_date>'2012-2-29';
SELECT * FROM date_table;