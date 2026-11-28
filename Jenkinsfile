pipeline {
    agent any

    options {
        timestamps()
        disableConcurrentBuilds()
    }

    parameters {
        string(
            name: 'SONAR_HOST_URL',
            defaultValue: 'http://localhost:9000',
            description: 'SonarQube server URL'
        )
    }

    stages {
        stage('Build Docker Environment') {
            steps {
                sh 'docker build -t parky-ci:latest .'
            }
        }

        stage('Compile & Strict Warnings') {
            steps {
                sh '''
                    docker run --rm \
                        --user "$(id -u):$(id -g)" \
                        --volume "$WORKSPACE:/app" \
                        --workdir /app \
                        parky-ci:latest \
                        sh -c 'make clean && make'
                '''
            }
        }

        stage('Headless Unit Tests & Benchmark') {
            steps {
                sh '''
                    docker run --rm \
                        --user "$(id -u):$(id -g)" \
                        --volume "$WORKSPACE:/app" \
                        --workdir /app \
                        parky-ci:latest \
                        sh -c 'make test && make bench'
                '''
            }
        }

        stage('Static Analysis & SonarQube Quality Gate') {
            steps {
                withCredentials([string(
                    credentialsId: 'SONAR_TOKEN',
                    variable: 'SONAR_TOKEN'
                )]) {
                    sh '''
                        set -eu
                        docker run --rm \
                            --user "$(id -u):$(id -g)" \
                            --volume "$WORKSPACE:/app" \
                            --workdir /app \
                            parky-ci:latest \
                            sh -c 'cppcheck --enable=warning,style,performance,portability --xml --xml-version=2 -I include src/ tests/ 2> cppcheck.xml'
                        docker run --rm -i \
                            --user "$(id -u):$(id -g)" \
                            --volume "$WORKSPACE:/app" \
                            --workdir /app \
                            parky-ci:latest \
                            python3 - <<'PY'
import json
import os
import xml.etree.ElementTree as ET

severity_map = {
    "error": "CRITICAL",
    "warning": "MAJOR",
    "style": "MINOR",
    "performance": "MINOR",
    "portability": "INFO",
    "information": "INFO",
}
issues = []
root = ET.parse("cppcheck.xml").getroot()
for error in root.findall("./errors/error"):
    locations = error.findall("./location")
    if not locations:
        continue
    location = locations[0]
    filename = location.get("file")
    if not filename:
        continue
    file_path = os.path.relpath(os.path.abspath(filename), os.getcwd())
    if file_path == ".." or file_path.startswith("../"):
        continue
    try:
        line = int(location.get("line", "1"))
    except ValueError:
        line = 1
    severity = error.get("severity", "warning").lower()
    message = error.get("verbose") or error.get("msg") or error.get("id", "cppcheck issue")
    issues.append({
        "engineId": "cppcheck",
        "ruleId": error.get("id", "unknown"),
        "type": "BUG" if severity in ("error", "warning") else "CODE_SMELL",
        "severity": severity_map.get(severity, "INFO"),
        "primaryLocation": {
            "message": message,
            "filePath": file_path,
            "textRange": {"startLine": max(line, 1)}
        }
    })

with open("sonar-cppcheck.json", "w", encoding="utf-8") as report:
    json.dump({"issues": issues}, report, indent=2)
PY
                        docker run --rm \
                            --network host \
                            --volume "$WORKSPACE:/usr/src" \
                            --workdir /usr/src \
                            --env SONAR_HOST_URL \
                            --env SONAR_TOKEN \
                            --env SONAR_USER_HOME=/tmp/sonar \
                            sonarsource/sonar-scanner-cli \
                            -Dsonar.qualitygate.wait=true
                    '''
                }
            }
        }
    }

    post {
        always {
            sh 'rm -f cppcheck.xml sonar-cppcheck.json'
        }
    }
}
