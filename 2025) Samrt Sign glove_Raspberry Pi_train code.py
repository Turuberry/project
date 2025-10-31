import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.neighbors import KNeighborsClassifier
from sklearn.metrics import classification_report
import joblib

# 1. 데이터 불러오기
data = pd.read_csv("jamo_copy.csv")

# 2. 특성과 레이블 분리
X = data[["f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8"]]
y = data["label"]

# 3. 학습/테스트 분할
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# 4. KNN 모델 정의 
model = KNeighborsClassifier(n_neighbors=1)
model.fit(X_train, y_train)

# 5. 예측 및 평가
y_pred = model.predict(X_test)
print("=== 분류 리포트 ===")
print(classification_report(y_test, y_pred))

# 6. 모델 저장
joblib.dump(model, "knn_model.pkl")
print("✅ 모델이 'knn_model.pkl'로 저장되었습니다.")
