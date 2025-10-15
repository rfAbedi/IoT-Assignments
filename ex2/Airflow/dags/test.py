import logging
import pendulum
from airflow import DAG
from airflow.providers.influxdb.hooks.influxdb import InfluxDBHook
from airflow.decorators import dag, task
from airflow.operators.python import PythonOperator
from airflow.providers.standard.operators.python import get_current_context

# Function to query InfluxDB and process temperature data
def process_temperature_data():
    # Initialize InfluxDB Hook
    hook = InfluxDBHook(
        influxdb_conn_id="influxdb_connection",  # Define your connection ID in Airflow
        influxdb_version="3",  # InfluxDB 3 is used
    )
    
    # SQL query to fetch temperature data from InfluxDB 3
    query = """
    SELECT * FROM sensors
    WHERE time > NOW() - INTERVAL 1 HOUR
    """
    
    # Fetch data from InfluxDB using the hook
    result = hook.query(query)
    
    # Process the result
    for record in result:
        temperature = record.get("t", None)

        if temperature is None:
            logging.error("No temperature data found.")
            continue

        # Check if temperature is within the Hot Path range
        if 15 <= temperature <= 25:
            logging.info(f"Valid Temperature: {temperature}°C within the Hot Path range.")
            # Process Hot Path data here (e.g., save it, log it, etc.)
        else:
            logging.info(f"Outlier Temperature: {temperature}°C. Flagged for outlier processing.")
            # Handle outliers (e.g., store separately, alert, etc.)

# Define the DAG with the TaskFlow API and templates
@dag(
    schedule="@daily",
    start_date=pendulum.datetime(2025, 5, 1, tz="UTC"),  # Adjust the start date as needed
    catchup=False,
    tags=["temperature", "hot_path"],
    params={"temperature_threshold_lower": 15, "temperature_threshold_upper": 25},
)
def temperature_processing_dag():
    """
    This DAG fetches temperature data from InfluxDB and processes it.
    It checks if the data is within the Hot Path range (15°C to 25°C) or an outlier.
    """
    
    @task(
        templates_exts=[".sql"],  # Allow SQL templates to be used
    )
    def template_test(sql, test_var, data_interval_end):
        context = get_current_context()

        # Print SQL and other context variables
        print(f"sql: {sql}")
        print(f"test_var: {test_var}")
        print(f"data_interval_end: {data_interval_end}")

        # Render and print template strings
        template_str = "run_id: {{ run_id }}; params.temperature_threshold_upper: {{ params.temperature_threshold_upper }}"
        rendered_template = context["task"].render_template(template_str, context)
        print(f"rendered template: {rendered_template}")

        print(f"context: {context}")

    # Override parameters with values specific to this task
    template_test.override(
        params={"temperature_threshold_upper": 30}  # Example override for upper temperature threshold
    )(
        sql="sql/temperature_query.sql",  # Example SQL template for query
        test_var="{{ run_id }}",
    )

    # Define the task to process temperature data
    process_temperature = PythonOperator(
        task_id='process_temperature',
        python_callable=process_temperature_data,
        provide_context=True,
    )

    # Define task dependencies if needed (e.g., run template_test before process_temperature)
    template_test() >> process_temperature

# Instantiate and invoke the DAG
temperature_processing_dag()
